package org.citra.citra_emu.features.settings.ui.viewholder;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.view.View;
import android.widget.TextView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.view.InputBindingSetting;
import org.citra.citra_emu.features.settings.model.view.SettingsItem;
import org.citra.citra_emu.features.settings.ui.SettingsAdapter;

public final class InputBindingSettingViewHolder extends SettingViewHolder {
    private InputBindingSetting mItem;

    private TextView mTextSettingName;
    private TextView mTextSettingDescription;

    private Context mContext;

    public InputBindingSettingViewHolder(View itemView, SettingsAdapter adapter, Context context) {
        super(itemView, adapter);

        mContext = context;
    }

    @Override
    protected void findViews(View root) {
        mTextSettingName = root.findViewById(R.id.text_setting_name);
        mTextSettingDescription = root.findViewById(R.id.text_setting_description);
    }

    @Override
    public void bind(SettingsItem item) {
        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(mContext);

        mItem = (InputBindingSetting) item;

        mTextSettingName.setText(item.getNameId());

        String key = sharedPreferences.getString(mItem.getKey(), "");
        if (key != null && !key.isEmpty()) {
            mTextSettingDescription.setText(key);
            mTextSettingDescription.setVisibility(View.VISIBLE);
        } else {
            mTextSettingDescription.setVisibility(View.GONE);
        }
    }

    @Override
    public void onClick(View clicked) {
        getAdapter().onInputBindingClick(mItem, getAdapterPosition());
    }
}
