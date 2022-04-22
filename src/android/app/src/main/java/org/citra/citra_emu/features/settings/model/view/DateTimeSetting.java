package org.citra.citra_emu.features.settings.model.view;

import org.citra.citra_emu.features.settings.model.Setting;
import org.citra.citra_emu.features.settings.model.StringSetting;

public final class DateTimeSetting extends SettingsItem {
    private String mDefaultValue;

    public DateTimeSetting(String key, String section, int titleId, int descriptionId,
                           String defaultValue, Setting setting) {
        super(key, section, setting, titleId, descriptionId);
        mDefaultValue = defaultValue;
    }

    public String getValue() {
        if (getSetting() != null) {
            StringSetting setting = (StringSetting) getSetting();
            return setting.getValue();
        } else {
            return mDefaultValue;
        }
    }

    public StringSetting setSelectedValue(String datetime) {
        if (getSetting() == null) {
            StringSetting setting = new StringSetting(getKey(), getSection(), datetime);
            setSetting(setting);
            return setting;
        } else {
            StringSetting setting = (StringSetting) getSetting();
            setting.setValue(datetime);
            return null;
        }
    }

    @Override
    public int getType() {
        return TYPE_DATETIME_SETTING;
    }
}