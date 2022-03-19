package org.citra.citra_emu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.view.CheckBoxSetting;
import org.citra.citra_emu.features.settings.model.view.SettingsItem;
import org.citra.citra_emu.features.settings.ui.SettingsAdapter;

public final class CheckBoxSettingViewHolder extends SettingViewHolder {
    private CheckBoxSetting mItem;

    private TextView mTextSettingName;
    private TextView mTextSettingDescription;

    private CheckBox mCheckbox;

    public CheckBoxSettingViewHolder(View itemView, SettingsAdapter adapter) {
        super(itemView, adapter);
    }

    @Override
    protected void findViews(View root) {
        mTextSettingName = root.findViewById(R.id.text_setting_name);
        mTextSettingDescription = root.findViewById(R.id.text_setting_description);
        mCheckbox = root.findViewById(R.id.checkbox);
    }

    @Override
    public void bind(SettingsItem item) {
        mItem = (CheckBoxSetting) item;

        mTextSettingName.setText(item.getNameId());

        if (item.getDescriptionId() > 0) {
            mTextSettingDescription.setText(item.getDescriptionId());
            mTextSettingDescription.setVisibility(View.VISIBLE);
        } else {
            mTextSettingDescription.setText("");
            mTextSettingDescription.setVisibility(View.GONE);
        }

        mCheckbox.setChecked(mItem.isChecked());
    }

    @Override
    public void onClick(View clicked) {
        mCheckbox.toggle();

        getAdapter().onBooleanClick(mItem, getAdapterPosition(), mCheckbox.isChecked());
    }
}
