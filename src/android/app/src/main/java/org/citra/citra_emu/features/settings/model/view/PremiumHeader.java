package org.citra.citra_emu.features.settings.model.view;

import org.citra.citra_emu.features.settings.model.Setting;

public final class PremiumHeader extends SettingsItem {
    public PremiumHeader() {
        super(null, null, null, 0, 0);
    }

    @Override
    public int getType() {
        return SettingsItem.TYPE_PREMIUM;
    }
}
