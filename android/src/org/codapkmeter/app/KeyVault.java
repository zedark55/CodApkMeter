package org.codapkmeter.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Device AES keys are encrypted with a non-exportable Android Keystore wrapping key. */
public final class KeyVault {
    private static final String ALIAS = "codapkmeter.wrapping.v1";
    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("meter_keys", Context.MODE_PRIVATE);
    }
    private static synchronized SecretKey wrappingKey() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey) store.getKey(ALIAS, null);
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS,
            KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true).build());
        return generator.generateKey();
    }
    public static boolean put(Context c, String id, String hex) {
        try {
            if (!hex.matches("[0-9a-fA-F]{32}")) return false;
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, wrappingKey());
            cipher.updateAAD(id.getBytes(StandardCharsets.UTF_8));
            byte[] data = cipher.doFinal(hex.getBytes(StandardCharsets.US_ASCII));
            String value = Base64.encodeToString(cipher.getIV(), Base64.NO_WRAP) + ":" +
                Base64.encodeToString(data, Base64.NO_WRAP);
            return prefs(c).edit().putString(id, value).commit();
        } catch (Exception e) { return false; }
    }
    public static String get(Context c, String id) {
        try {
            String value = prefs(c).getString(id, null);
            if (value == null) return "";
            String[] parts = value.split(":", -1);
            if (parts.length != 2) return "";
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, wrappingKey(),
                new GCMParameterSpec(128, Base64.decode(parts[0], Base64.NO_WRAP)));
            cipher.updateAAD(id.getBytes(StandardCharsets.UTF_8));
            return new String(cipher.doFinal(Base64.decode(parts[1], Base64.NO_WRAP)), StandardCharsets.US_ASCII);
        } catch (Exception e) { return ""; }
    }
    public static boolean remove(Context c, String id) {
        return prefs(c).edit().remove(id).commit();
    }
    private KeyVault() {}
}
