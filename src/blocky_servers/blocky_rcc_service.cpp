/**
 * BlockyRccService — il server dedicato come servizio Windows vero.
 *
 * ── Da dove viene il nome ──────────────────────────────────────────────────
 * RCCService era il processo che Roblox usava per tenere in piedi i server
 * dedicati senza che nessuno dovesse restare davanti a una finestra di
 * console: partiva da solo con Windows, girava senza farsi vedere, e se
 * moriva la Service Control Manager lo faceva ripartire. BlockyServer.exe
 * fa la stessa cosa ma solo finche' qualcuno lo tiene aperto in una console:
 * chiudi quella finestra, chiudi il server. Questo file e' l'equivalente
 * Blocky di RCCService.
 *
 * ── Cosa fa DAVVERO un servizio Windows, che un .bat non puo' fare ────────
 *   - Parte all'avvio di Windows, ANCHE PRIMA che qualcuno faccia login.
 *   - Gira senza nessuna finestra: non e' "minimizzato", non esiste proprio
 *     una finestra da minimizzare.
 *   - Se il processo muore, la SCM lo fa ripartire da sola (configurato qui
 *     sotto in installService(), SC_ACTION_RESTART) — nessuno se ne accorge.
 *   - Si ferma in modo pulito con `sc stop BlockyRCC` o dal pannello
 *     Servizi di Windows, invece di dover uccidere il processo a mano.
 *
 * ── Perche' e' un file cosi' diverso dal resto del motore ──────────────────
 * La Service Control Manager (SCM) di Windows ha delle regole rigide: il
 * processo deve chiamare StartServiceCtrlDispatcher entro pochi secondi
 * dall'avvio o la SCM lo considera bloccato e lo uccide; il "vero" ciclo di
 * gioco (runDedicatedServerLoop, condiviso con blocky_server.cpp tramite
 * dedicated_server_core.h) parte solo DENTRO ServiceMain, sul thread che la
 * SCM stessa fornisce. Questo file e' quindi quasi tutto burocrazia SCM: la
 * logica di rete vera resta nell'header condiviso, invariata.
 *
 * ── Uso ────────────────────────────────────────────────────────────────────
 *   BlockyRccService.exe --install [--port N] [--max N] [--name STR] [--game N]
 *       Registra il servizio. Richiede un prompt da AMMINISTRATORE: la SCM
 *       non permette di registrare servizi a un utente normale, e non c'e'
 *       modo di aggirarlo — ne' dovrebbe esserci, e' li' apposta.
 *   BlockyRccService.exe --uninstall
 *       Toglie il servizio (lo ferma prima, se era in esecuzione).
 *   BlockyRccService.exe --console [--port N] [--max N] [--name STR]
 *       Fa girare lo stesso ciclo in una console normale, per provarlo senza
 *       installarlo — utile per verificare la configurazione prima di
 *       registrarlo per davvero.
 *   (nessun argomento riconosciuto)
 *       Assume di essere stato avviato dalla SCM e chiama
 *       StartServiceCtrlDispatcher. Se non lo era (es. doppio click per
 *       sbaglio), quella chiamata fallisce e stampiamo un messaggio chiaro
 *       invece di restare li' a non fare nulla.
 */

#include "dedicated_server_core.h"
#include "platform/Platform.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

constexpr const char* kServiceName = "BlockyRCC";
constexpr const char* kDisplayName = "Blocky RCC Service";
constexpr const char* kDescription =
    "Server dedicato multiplayer di Blocky, sempre attivo. "
    "Equivalente Blocky del vecchio RCCService di Roblox.";

// Riempita in main() PRIMA di StartServiceCtrlDispatcher, letta da
// ServiceMain quando la SCM lo invoca. Non puo' arrivare per parametro:
// ServiceMain ha la firma imposta dalla SCM (DWORD, LPSTR*), e quei LPSTR*
// sono argomenti passati con `sc start`, non quelli scritti nel binPath —
// per questo la configurazione viaggia nel binPath stesso (vedi
// installService) e viene lettA QUI da main() come un avvio normale.
DedicatedServerConfig g_config;

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
std::atomic<bool> g_keepRunning{true};

/** Il file di log: un servizio non ha una console su cui stampare. */
std::ofstream& logFile() {
    static std::ofstream f;
    static bool opened = false;
    if (!opened) {
        opened = true;
        std::string path = plat::dataDir() + "/rcc_service.log";
        f.open(path, std::ios::app);
    }
    return f;
}

void logLine(const std::string& line) {
    std::ofstream& f = logFile();
    if (!f) return;
    f << line << "\n";
    f.flush();   // un servizio che muore non deve perdere l'ultima riga
}

void reportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0) {
    if (!g_statusHandle) return;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint = waitHint;
    g_status.dwControlsAccepted = (state == SERVICE_START_PENDING)
        ? 0 : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    SetServiceStatus(g_statusHandle, &g_status);
}

/**
 * Il control handler: qui arrivano stop/shutdown dalla SCM, su un thread
 * suo. Si limita a mettere keepRunning a false — e' l'ATOMIC che rende
 * sicuro leggerlo dal thread del ciclo di gioco senza una lock.
 */
DWORD WINAPI serviceCtrlHandler(DWORD ctrl, DWORD, LPVOID, LPVOID) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
        g_keepRunning.store(false, std::memory_order_relaxed);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

/**
 * Chiamata dalla SCM su un thread suo, dopo StartServiceCtrlDispatcher.
 * Gira DENTRO questa funzione per tutta la vita del servizio: non serve un
 * thread separato, perche' il control handler (sopra) gira gia' su un
 * thread diverso fornito dalla SCM stessa.
 */
void WINAPI serviceMain(DWORD, char**) {
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwServiceSpecificExitCode = 0;

    g_statusHandle = RegisterServiceCtrlHandlerExA(kServiceName, serviceCtrlHandler, nullptr);
    if (!g_statusHandle) return;

    reportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    LanNet net;
    if (!startDedicatedServer(net, g_config, logLine, /*showCtrlCHint=*/false)) {
        reportStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
        return;
    }

    reportStatus(SERVICE_RUNNING);
    logLine("[RCC] Servizio avviato.");

    runDedicatedServerLoop(net, g_config.maxPlayers, g_keepRunning, logLine);

    logLine("[RCC] Servizio fermato.");
    reportStatus(SERVICE_STOPPED);
}

bool isElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev{};
    DWORD size = sizeof(elev);
    bool ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size) != 0;
    CloseHandle(token);
    return ok && elev.TokenIsElevated != 0;
}

/**
 * La configurazione viaggia nel binPath del servizio, non altrove: e' quello
 * che la SCM ripassa al processo ogni volta che lo avvia, quindi main()
 * la rilegge con lo stesso parseServerArgs di sempre a ogni riavvio — nessun
 * file di configurazione a parte da tenere sincronizzato.
 */
bool installService(const DedicatedServerConfig& cfg) {
    if (!isElevated()) {
        std::fprintf(stderr,
            "Serve un prompt da AMMINISTRATORE per registrare un servizio.\n"
            "Windows non permette di aggirare questo, ed e' corretto cosi'.\n");
        return false;
    }

    const std::string exePath = plat::selfPath();
    if (exePath.empty()) {
        std::fprintf(stderr, "Non trovo il percorso del mio stesso eseguibile.\n");
        return false;
    }

    std::string binPath = "\"" + exePath + "\""
        + " --port " + std::to_string(cfg.port)
        + " --max " + std::to_string(cfg.maxPlayers)
        + " --name \"" + cfg.name + "\"";
    if (cfg.gameIndex >= 0) binPath += " --game " + std::to_string(cfg.gameIndex);

    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        std::fprintf(stderr, "OpenSCManager fallita (codice %lu).\n", GetLastError());
        return false;
    }

    SC_HANDLE svc = CreateServiceA(
        scm, kServiceName, kDisplayName,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,     // parte da solo con Windows
        SERVICE_ERROR_NORMAL,
        binPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            std::fprintf(stderr, "Il servizio esiste gia'. Disinstallalo prima con --uninstall\n"
                                  "se vuoi cambiare porta/nome/max giocatori.\n");
        } else {
            std::fprintf(stderr, "CreateService fallita (codice %lu).\n", err);
        }
        CloseServiceHandle(scm);
        return false;
    }

    // Descrizione visibile in services.msc.
    SERVICE_DESCRIPTIONA desc{ (LPSTR)kDescription };
    ChangeServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

    // Se il processo muore, la SCM lo fa ripartire da sola. E' la differenza
    // vera fra un servizio e un .bat: nessuno deve accorgersi che e' morto.
    SC_ACTION actions[3] = {
        { SC_ACTION_RESTART, 5000  },   // 1o crash: riparte dopo 5s
        { SC_ACTION_RESTART, 15000 },   // 2o: aspetta un po' di piu'
        { SC_ACTION_RESTART, 60000 },   // dal 3o in poi: non martellare
    };
    SERVICE_FAILURE_ACTIONSA fail{};
    fail.dwResetPeriod = 86400;   // il contatore torna a zero dopo un giorno senza crash
    fail.cActions = 3;
    fail.lpsaActions = actions;
    ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fail);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    std::printf("Servizio '%s' registrato.\n", kServiceName);
    std::printf("  Porta %d, max %d giocatori, nome \"%s\"\n",
               cfg.port, cfg.maxPlayers, cfg.name.c_str());
    std::printf("Si avvia da solo al prossimo riavvio di Windows, oppure adesso con:\n");
    std::printf("  sc start %s\n", kServiceName);
    return true;
}

bool uninstallService() {
    if (!isElevated()) {
        std::fprintf(stderr, "Serve un prompt da AMMINISTRATORE per rimuovere un servizio.\n");
        return false;
    }

    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        std::fprintf(stderr, "OpenSCManager fallita (codice %lu).\n", GetLastError());
        return false;
    }
    SC_HANDLE svc = OpenServiceA(scm, kServiceName, SERVICE_STOP | DELETE);
    if (!svc) {
        std::fprintf(stderr, "Il servizio '%s' non risulta installato.\n", kServiceName);
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS st{};
    ControlService(svc, SERVICE_CONTROL_STOP, &st);   // se non gira, fallisce e va bene cosi'

    bool ok = DeleteService(svc) != 0;
    if (!ok) std::fprintf(stderr, "DeleteService fallita (codice %lu).\n", GetLastError());
    else std::printf("Servizio '%s' rimosso.\n", kServiceName);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

/** --console: stesso ciclo, ma su una console normale — per provare prima di installare. */
int runConsole(const DedicatedServerConfig& cfg) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const ServerLogFn consoleLog = [](const std::string& line) {
        std::printf("%s\n", line.c_str());
    };
    LanNet net;
    if (!startDedicatedServer(net, cfg, consoleLog)) return 1;
    std::atomic<bool> keepRunning{true};
    runDedicatedServerLoop(net, cfg.maxPlayers, keepRunning, consoleLog);
    return 0;
}

void printServiceHelp() {
    std::printf(
        "BlockyRccService — server dedicato come servizio Windows\n"
        "  --install [--port N] [--max N] [--name STR] [--game N]\n"
        "      Registra il servizio (richiede amministratore).\n"
        "  --uninstall\n"
        "      Rimuove il servizio (richiede amministratore).\n"
        "  --console [--port N] [--max N] [--name STR] [--game N]\n"
        "      Prova lo stesso ciclo in una console, senza installare nulla.\n"
        "\nSenza argomenti: assume di essere avviato dalla Service Control\n"
        "Manager di Windows.\n");
}

} // namespace

int main(int argc, char** argv) {
    bool doInstall = false, doUninstall = false, doConsole = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--install") == 0) doInstall = true;
        else if (std::strcmp(argv[i], "--uninstall") == 0) doUninstall = true;
        else if (std::strcmp(argv[i], "--console") == 0) doConsole = true;
    }

    // parseServerArgs ignora silenziosamente i flag che non riconosce
    // (--install/--uninstall/--console), quindi si puo' passare l'argv
    // intero e ottenere comunque porta/max/nome/game corretti.
    bool wantsHelp = false;
    parseServerArgs(argc, argv, g_config, wantsHelp);
    if (wantsHelp) { printServiceHelp(); return 0; }

    if (doUninstall) return uninstallService() ? 0 : 1;
    if (doInstall)   return installService(g_config) ? 0 : 1;
    if (doConsole)   return runConsole(g_config);

    // Nessun flag riconosciuto: assumiamo di essere stati avviati dalla SCM.
    SERVICE_TABLE_ENTRYA table[] = {
        { (LPSTR)kServiceName, (LPSERVICE_MAIN_FUNCTIONA)serviceMain },
        { nullptr, nullptr }
    };
    if (!StartServiceCtrlDispatcherA(table)) {
        std::fprintf(stderr,
            "BlockyRccService non e' stato avviato dalla Service Control Manager.\n"
            "Usa --install per registrarlo come servizio, oppure --console per\n"
            "provarlo in una finestra normale. --help per l'elenco completo.\n");
        return 1;
    }
    return 0;
}
