package com.blocky.preview

import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import java.io.File
import java.io.FileOutputStream
import com.google.androidgamesdk.GameActivity

/**
 * Il motore vero gira in C++ (android_entry.cpp): GameActivity gestisce ciclo
 * di vita, finestra nativa e input, e li passa ad android_main().
 *
 * Qui resta solo quello che il C++ non puo' fare da solo, cioe' mostrare una
 * finestra di dialogo di sistema.
 */
class MainActivity : GameActivity() {

    /**
     * Messaggio di errore irrecuperabile, chiamato dal C++ via JNI.
     *
     * Serve perche' certi guasti impediscono al motore di partire del tutto: se
     * non si puo' creare un contesto grafico, non si puo' nemmeno disegnare un
     * avviso a schermo col nostro sistema di interfaccia. Senza questo, chi ha
     * un dispositivo non supportato vedrebbe solo uno schermo nero, senza
     * nessuna idea del perche'.
     *
     * Il dialogo va costruito sul thread dell'interfaccia: android_main() gira
     * su un thread suo, quindi la chiamata viene rimandata con runOnUiThread.
     */
    @Suppress("unused")   // chiamata da android_bridge.cpp, non dal codice Kotlin
    fun showFatalMessage(message: String) {
        runOnUiThread {
            if (isFinishing || isDestroyed) return@runOnUiThread
            AlertDialog.Builder(this)
                .setTitle("Blocky")
                .setMessage(message)
                .setCancelable(false)
                .setPositiveButton("Chiudi") { _, _ -> finish() }
                .show()
        }
    }


    // ------------------------------------------------------------------
    //  Firma dell'APK: l'anti-manomissione vero per Android
    // ------------------------------------------------------------------
    //
    // Su Windows Integrity.h legge l'hash del proprio .exe e la sezione
    // .text: cose specifiche del formato PE, che su Android non hanno senso
    // (e' una .so dentro un APK, non un eseguibile a se'). Il controllo
    // GIUSTO per Android e' un altro: chi ha firmato l'APK installato.
    //
    // Un APK modificato (una hack ricompilata) deve essere firmato di nuovo,
    // e chi lo modifica non ha la chiave privata originale — puo' solo
    // firmarlo con una SUA chiave. Leggendo l'impronta della firma a runtime
    // e confrontandola con quella attesa (compilata in Integrity.h) si
    // scopre esattamente questo: non IMPEDISCE di modificare l'APK, ma lo fa
    // presentarsi con un'impronta diversa, che il gate di rete rifiuta —
    // lo stesso principio delle impronte dei file su PC, applicato alla
    // firma invece che al contenuto.
    //
    // Due percorsi perche minSdk e' 24: GET_SIGNING_CERTIFICATES esiste solo
    // da API 28. Sotto, si usa la API vecchia (deprecata ma ancora
    // funzionante) — un'app puo' avere piu' firmatari solo con la API nuova
    // (firma multipla), che qui non serve: Blocky si firma con una chiave sola.
    @Suppress("unused", "DEPRECATION")   // chiamata da android_bridge.cpp
    fun signingCertSha256(): String {
        return try {
            val sig = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val info = packageManager.getPackageInfo(
                    packageName, android.content.pm.PackageManager.GET_SIGNING_CERTIFICATES)
                val signers = info.signingInfo?.apkContentsSigners
                if (signers.isNullOrEmpty()) return "" else signers[0]
            } else {
                val info = packageManager.getPackageInfo(
                    packageName, android.content.pm.PackageManager.GET_SIGNATURES)
                val signers = info.signatures
                if (signers.isNullOrEmpty()) return "" else signers[0]
            }
            val digest = java.security.MessageDigest.getInstance("SHA-256").digest(sig.toByteArray())
            digest.joinToString("") { "%02X".format(it) }
        } catch (e: Exception) {
            ""   // vuoto = "non verificabile", MAI un valore finto che sembri valido
        }
    }

    // ------------------------------------------------------------------
    //  Foreground Service: tenere vivo il processo mentre si ospita
    // ------------------------------------------------------------------
    //
    // Chiamate da LanNet.h (codice condiviso col PC) tramite il gancio
    // notifyHostingStateChanged installato in android_bridge.cpp — LanNet
    // non sa che esiste Android, chiama solo il gancio, che su PC non fa
    // nulla. Vedi BlockyHostService.kt per cosa fa davvero questo servizio
    // e cosa NON fa ancora.

    @Suppress("unused")   // chiamata da android_bridge.cpp
    fun startHostForegroundService() {
        BlockyHostService.start(this)
    }

    @Suppress("unused")   // chiamata da android_bridge.cpp
    fun stopHostForegroundService() {
        BlockyHostService.stop(this)
    }

    // ------------------------------------------------------------------
    //  Selettore file
    // ------------------------------------------------------------------
    //
    // E' l'unica funzione del motore che Android obbliga a fare in Java, e
    // non perche' manchi una traduzione: su Android 15 l'app NON PUO' leggere
    // i file dell'utente. Provato: `ls /sdcard/Download` risponde "Permission
    // denied". L'unica via e' chiedere al sistema di mostrare il SUO
    // selettore; l'utente sceglie, e il sistema ci consegna un permesso
    // temporaneo su quel singolo file.
    //
    // Da qui le due differenze che nessuno strumento potrebbe convertire da
    // solo: la risposta arriva DOPO (onActivityResult, non un valore di
    // ritorno), e arriva come content:// invece che come percorso. Il file
    // viene quindi copiato nella cache dell'app, e al C++ torna un percorso
    // normale — cosi' il motore continua a vedere solo percorsi, come su PC.

    private val pickRequestCode = 4711

    @Suppress("unused")   // chiamata da android_bridge.cpp
    fun openFilePicker(mimeType: String) {
        runOnUiThread {
            if (isFinishing || isDestroyed) { nativeFilePicked(""); return@runOnUiThread }
            try {
                val i = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = if (mimeType.isEmpty()) "*/*" else mimeType
                }
                startActivityForResult(i, pickRequestCode)
            } catch (e: Exception) {
                // Nessuna app in grado di aprire file: si risponde "niente"
                // invece di lasciare il motore in attesa per sempre.
                nativeFilePicked("")
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != pickRequestCode) return

        val uri = if (resultCode == RESULT_OK) data?.data else null
        if (uri == null) { nativeFilePicked(""); return }   // annullato

        // La copia puo' essere lenta (file grandi, cloud): fuori dal thread
        // dell'interfaccia, o l'app si blocca mentre copia.
        Thread {
            var out = ""
            try {
                val name = queryDisplayName(uri) ?: "modello.glb"
                val dest = File(cacheDir, "picked_" + System.currentTimeMillis() + "_" + name)
                contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(dest).use { output -> input.copyTo(output) }
                }
                if (dest.exists() && dest.length() > 0) out = dest.absolutePath
            } catch (e: Exception) {
                out = ""
            }
            nativeFilePicked(out)
        }.start()
    }

    /** Il nome che l'utente vede, per conservare l'estensione giusta. */
    private fun queryDisplayName(uri: Uri): String? {
        return try {
            contentResolver.query(uri, null, null, null, null)?.use { c ->
                val idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (idx >= 0 && c.moveToFirst()) c.getString(idx) else null
            }
        } catch (e: Exception) { null }
    }

    /** Percorso scelto (vuoto = annullato). Implementata in android_bridge.cpp. */
    private external fun nativeFilePicked(path: String)
    companion object {
        init {
            System.loadLibrary("blocky")
        }
    }
}
