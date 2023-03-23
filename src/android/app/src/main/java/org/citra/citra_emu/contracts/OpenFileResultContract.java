package org.citra.citra_emu.contracts;

import android.content.Context;
import android.content.Intent;
import android.util.Pair;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class OpenFileResultContract extends ActivityResultContract<Boolean, Intent> {
    @NonNull
    @Override
    public Intent createIntent(@NonNull Context context, Boolean allowMultiple) {
        return new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .setType("application/octet-stream")
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultiple);
    }

    @Override
    public Intent parseResult(int i, @Nullable Intent intent) {
        return intent;
    }
}
