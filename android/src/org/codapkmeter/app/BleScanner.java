package org.codapkmeter.app;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.ParcelUuid;
import android.util.Log;
import android.util.SparseArray;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.util.ArrayDeque;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/** Foreground-only, low latency advertising scan. Never connects to a meter. */
public final class BleScanner {
    private static final String TAG = "ChronosBLE";
    private static final int QUEUE_LIMIT = 4096;
    private static final int BATCH_LIMIT = 256;
    private static final ArrayDeque<JSONObject> pending = new ArrayDeque<>();
    private static BluetoothLeScanner scanner;
    private static BluetoothAdapter adapter;
    private static ScanCallback callback;
    private static String error = "";
    private static int dropped;

    public static synchronized String start(Context context) {
        stop();
        pending.clear(); error = ""; dropped = 0;
        try {
            BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
            adapter = manager == null ? null : manager.getAdapter();
            if (adapter == null || !adapter.isEnabled()) return "Включите Bluetooth в настройках телефона";
            scanner = adapter.getBluetoothLeScanner();
            if (scanner == null) return "BLE-сканер недоступен";
            callback = new ScanCallback() {
                @Override public void onScanResult(int type, ScanResult result) { receive(this, result); }
                @Override public void onBatchScanResults(List<ScanResult> results) {
                    for (ScanResult result : results) receive(this, result);
                }
                @Override public void onScanFailed(int code) {
                    synchronized (BleScanner.class) {
                        if (callback != this) return;
                        error = "Ошибка BLE-сканирования Android: " + code;
                        Log.e(TAG, error);
                    }
                }
            };
            ScanSettings settings = new ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                    .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                    .setReportDelay(0)
                    .build();
            // No name, UUID, connectability or Company ID filter: meters may
            // advertise manufacturer data only, without flags or a local name.
            scanner.startScan(Collections.<ScanFilter>emptyList(), settings, callback);
            Log.i(TAG, "Started foreground BLE scan: LOW_LATENCY, ALL_MATCHES, reportDelay=0");
            return "";
        } catch (RuntimeException ex) {
            stop();
            Log.e(TAG, "Cannot start BLE scan", ex);
            return "Не удалось запустить BLE-сканер: " + ex.getClass().getSimpleName();
        }
    }

    public static synchronized void stop() {
        ScanCallback old = callback;
        callback = null; // Ignore late callbacks from a previous session.
        if (scanner != null && old != null) {
            try { scanner.stopScan(old); }
            catch (RuntimeException ex) { Log.w(TAG, "Cannot stop BLE scan", ex); }
            Log.i(TAG, "Stopped BLE scan");
        }
        scanner = null;
        adapter = null;
        pending.clear();
    }

    private static String hex(byte[] data) {
        char[] chars = new char[data.length * 2];
        final char[] digits = "0123456789ABCDEF".toCharArray();
        for (int i = 0; i < data.length; i++) {
            chars[i * 2] = digits[(data[i] & 255) >>> 4];
            chars[i * 2 + 1] = digits[data[i] & 15];
        }
        return new String(chars);
    }

    private static void enqueue(String source, int company, String service, byte[] data,
                                ScanResult result) throws JSONException {
        if (data == null || data.length == 0 || data.length > 1650) return;
        JSONObject packet = new JSONObject();
        packet.put("source", source); packet.put("company", company);
        packet.put("service", service); packet.put("hex", hex(data));
        packet.put("rssi", result.getRssi());
        packet.put("address", result.getDevice().getAddress());
        if (pending.size() >= QUEUE_LIMIT) { pending.removeFirst(); dropped++; }
        pending.addLast(packet);
    }

    private static synchronized void receive(ScanCallback owner, ScanResult result) {
        if (owner != callback || result == null) return;
        ScanRecord record = result.getScanRecord();
        if (record == null) return;
        try {
            SparseArray<byte[]> manufacturer = record.getManufacturerSpecificData();
            for (int i = 0; manufacturer != null && i < manufacturer.size(); i++) {
                enqueue("manufacturer", manufacturer.keyAt(i), "", manufacturer.valueAt(i), result);
                if (manufacturer.keyAt(i) == 0x168F && manufacturer.valueAt(i) != null)
                    Log.d(TAG, "ETO advertisement: bytes=" + manufacturer.valueAt(i).length + " rssi=" + result.getRssi());
            }
            Map<ParcelUuid, byte[]> services = record.getServiceData();
            if (services != null) for (Map.Entry<ParcelUuid, byte[]> entry : services.entrySet())
                enqueue("service", -1, entry.getKey().toString(), entry.getValue(), result);
        } catch (JSONException | SecurityException ex) {
            error = "Ошибка чтения BLE-пакета: " + ex.getClass().getSimpleName();
            Log.e(TAG, error, ex);
        }
    }

    /** Qt polls in bounded batches; Android callback never touches Qt objects. */
    public static synchronized String drain() {
        try {
            if (callback != null && adapter != null && !adapter.isEnabled()) error = "Включите Bluetooth в настройках телефона";
        } catch (SecurityException ex) { error = "Разрешите доступ к устройствам поблизости"; }
        JSONArray packets = new JSONArray();
        for (int i = 0; i < BATCH_LIMIT && !pending.isEmpty(); i++) packets.put(pending.removeFirst());
        JSONObject batch = new JSONObject();
        try {
            batch.put("packets", packets); batch.put("error", error); batch.put("dropped", dropped);
        } catch (JSONException ex) { Log.e(TAG, "Cannot serialize scan batch", ex); }
        error = ""; dropped = 0;
        return batch.toString();
    }
}
