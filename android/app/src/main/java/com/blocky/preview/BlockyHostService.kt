package com.blocky.preview

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat

/**
 * Tiene vivo il PROCESSO mentre il telefono ospita una partita, cosi'
 * Android non lo sospende ne' lo rallenta come farebbe con una qualunque
 * app messa in secondo piano (Doze, App Standby).
 *
 * ── Cosa NON fa, e va detto chiaro ─────────────────────────────────────────
 * Non rimette da sola in moto il tick di rete se GameActivity ha perso la
 * superficie di disegno — schermo spento, app scambiata con un'altra. Quel
 * momento fa scattare APP_CMD_TERM_WINDOW (vedi android_entry.cpp), che
 * ferma blockyAndroidFrame() e con esso g_net.poll(): tenere vivo il
 * processo non basta a farlo ripartire da solo. Farlo per davvero vuol dire
 * sganciare il tick di rete dal ciclo di rendering, con un mutex attorno a
 * g_net — lavoro dichiarato e non ancora fatto (vedi il commento sul gancio
 * in platform/Platform.h), non nascosto sotto un servizio che sembra
 * risolvere tutto e in realta' tiene vivo solo il contenitore.
 *
 * Quello che invece FA gia', per davvero: appena l'ospite riporta l'app in
 * primo piano (schermo riacceso, GameActivity con la superficie tornata
 * valida), il processo non e' mai stato ucciso da Android nel frattempo, e
 * net.poll() riprende esattamente da dove il tick di rendering lo aveva
 * lasciato — non da un processo ripartito da zero.
 *
 * La notifica persistente NON e' facoltativa: da Android 8 in poi la legge
 * del sistema operativo la richiede per ogni Foreground Service, visibile
 * finche' il servizio gira. Non e' una scelta di design, e' un vincolo di
 * piattaforma — l'utente sa sempre che qualcosa gira in background.
 */
class BlockyHostService : Service() {

    private var wakeLock: PowerManager.WakeLock? = null

    override fun onCreate() {
        super.onCreate()
        createChannelIfNeeded()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val notification = buildNotification()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // CONNECTED_DEVICE: il tipo che descrive meglio cosa fa davvero
            // questo servizio — parlare in rete con altri giocatori — fra
            // quelli che Android riconosce per un Foreground Service.
            startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(NOTIF_ID, notification)
        }

        if (wakeLock == null) {
            val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
            // PARTIAL: la CPU resta sveglia, lo schermo puo' comunque
            // spegnersi — e' esattamente cio' che serve per continuare a
            // rispondere alla rete senza dover tenere lo schermo acceso.
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Blocky:hosting").apply {
                setReferenceCounted(false)
                // Scadenza esplicita: un wake lock senza timeout che non
                // viene mai rilasciato per un bug altrove terrebbe la
                // batteria sveglia per sempre. Quattro ore coprono una
                // sessione di gioco lunga; oltre, l'host lo riavvia da capo.
                acquire(4 * 60 * 60 * 1000L)
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createChannelIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val mgr = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (mgr.getNotificationChannel(CHANNEL_ID) != null) return
        val channel = NotificationChannel(
            CHANNEL_ID, "Server Blocky", NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Attivo mentre ospiti una partita in background"
        }
        mgr.createNotificationChannel(channel)
    }

    private fun buildNotification(): Notification {
        val openIntent = packageManager.getLaunchIntentForPackage(packageName)
        val flags = PendingIntent.FLAG_UPDATE_CURRENT or
            (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
        val pending = PendingIntent.getActivity(this, 0, openIntent, flags)

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Blocky — server attivo")
            .setContentText("Stai ospitando una partita")
            // L'icona del lanciatore, gia' presente nel progetto: un'icona a
            // colori nella barra di stato puo' apparire come una semplice
            // silhouette su alcune versioni di Android — corretto comunque,
            // solo esteticamente meno rifinito di un'icona monocromatica
            // dedicata, che si puo' aggiungere in un giro successivo.
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentIntent(pending)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val CHANNEL_ID = "blocky_hosting"
        private const val NOTIF_ID = 1

        fun start(ctx: Context) {
            val i = Intent(ctx, BlockyHostService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) ctx.startForegroundService(i)
            else ctx.startService(i)
        }

        fun stop(ctx: Context) {
            ctx.stopService(Intent(ctx, BlockyHostService::class.java))
        }
    }
}
