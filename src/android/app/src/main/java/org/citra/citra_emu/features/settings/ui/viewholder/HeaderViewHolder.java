package org.citra.citra_emu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.view.SettingsItem;
import org.citra.citra_emu.features.settings.ui.SettingsAdapter;

public final class HeaderViewHolder extends SettingViewHolder {
    private TextView mHeaderName;

    public HeaderViewHolder(View itemView, SettingsAdapter adapter) {
        super(itemView, adapter);
        itemView.setOnClickListener(null);
    }

    @Override
    protected void findViews(View root) {
        mHeaderName = root.findViewById(R.id.text_header_name);
    }

    @Override
    public void bind(SettingsItem item) {
        mHeaderName.setText(item.getNameId());
    }

    @Override
    public void onClick(View clicked) {
        // no-op
    }
}