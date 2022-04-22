package org.citra.citra_emu.features.settings.ui;

import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.DatePicker;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.TimePicker;

import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.RecyclerView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.dialogs.MotionAlertDialog;
import org.citra.citra_emu.features.settings.model.FloatSetting;
import org.citra.citra_emu.features.settings.model.IntSetting;
import org.citra.citra_emu.features.settings.model.StringSetting;
import org.citra.citra_emu.features.settings.model.view.CheckBoxSetting;
import org.citra.citra_emu.features.settings.model.view.DateTimeSetting;
import org.citra.citra_emu.features.settings.model.view.InputBindingSetting;
import org.citra.citra_emu.features.settings.model.view.PremiumSingleChoiceSetting;
import org.citra.citra_emu.features.settings.model.view.SettingsItem;
import org.citra.citra_emu.features.settings.model.view.SingleChoiceSetting;
import org.citra.citra_emu.features.settings.model.view.SliderSetting;
import org.citra.citra_emu.features.settings.model.view.StringSingleChoiceSetting;
import org.citra.citra_emu.features.settings.model.view.SubmenuSetting;
import org.citra.citra_emu.features.settings.ui.viewholder.CheckBoxSettingViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.DateTimeViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.HeaderViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.InputBindingSettingViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.PremiumViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.SettingViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.SingleChoiceViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.SliderViewHolder;
import org.citra.citra_emu.features.settings.ui.viewholder.SubmenuViewHolder;
import org.citra.citra_emu.ui.main.MainActivity;
import org.citra.citra_emu.utils.Log;

import java.util.ArrayList;

public final class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
        implements DialogInterface.OnClickListener, SeekBar.OnSeekBarChangeListener {
    private SettingsFragmentView mView;
    private Context mContext;
    private ArrayList<SettingsItem> mSettings;

    private SettingsItem mClickedItem;
    private int mClickedPosition;
    private int mSeekbarProgress;

    private AlertDialog mDialog;
    private TextView mTextSliderValue;

    public SettingsAdapter(SettingsFragmentView view, Context context) {
        mView = view;
        mContext = context;
        mClickedPosition = -1;
    }

    @Override
    public SettingViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view;
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        switch (viewType) {
            case SettingsItem.TYPE_HEADER:
                view = inflater.inflate(R.layout.list_item_settings_header, parent, false);
                return new HeaderViewHolder(view, this);

            case SettingsItem.TYPE_CHECKBOX:
                view = inflater.inflate(R.layout.list_item_setting_checkbox, parent, false);
                return new CheckBoxSettingViewHolder(view, this);

            case SettingsItem.TYPE_SINGLE_CHOICE:
            case SettingsItem.TYPE_STRING_SINGLE_CHOICE:
                view = inflater.inflate(R.layout.list_item_setting, parent, false);
                return new SingleChoiceViewHolder(view, this);

            case SettingsItem.TYPE_SLIDER:
                view = inflater.inflate(R.layout.list_item_setting, parent, false);
                return new SliderViewHolder(view, this);

            case SettingsItem.TYPE_SUBMENU:
                view = inflater.inflate(R.layout.list_item_setting, parent, false);
                return new SubmenuViewHolder(view, this);

            case SettingsItem.TYPE_INPUT_BINDING:
                view = inflater.inflate(R.layout.list_item_setting, parent, false);
                return new InputBindingSettingViewHolder(view, this, mContext);

            case SettingsItem.TYPE_DATETIME_SETTING:
                view = inflater.inflate(R.layout.list_item_setting, parent, false);
                return new DateTimeViewHolder(view, this);

            case SettingsItem.TYPE_PREMIUM:
                view = inflater.inflate(R.layout.premium_item_setting, parent, false);
                return new PremiumViewHolder(view, this, mView);

            default:
                Log.error("[SettingsAdapter] Invalid view type: " + viewType);
                return null;
        }
    }

    @Override
    public void onBindViewHolder(SettingViewHolder holder, int position) {
        holder.bind(getItem(position));
    }

    private SettingsItem getItem(int position) {
        return mSettings.get(position);
    }

    @Override
    public int getItemCount() {
        if (mSettings != null) {
            return mSettings.size();
        } else {
            return 0;
        }
    }

    @Override
    public int getItemViewType(int position) {
        return getItem(position).getType();
    }

    public void setSettings(ArrayList<SettingsItem> settings) {
        mSettings = settings;
        notifyDataSetChanged();
    }

    public void onBooleanClick(CheckBoxSetting item, int position, boolean checked) {
        IntSetting setting = item.setChecked(checked);
        notifyItemChanged(position);

        if (setting != null) {
            mView.putSetting(setting);
        }

        mView.onSettingChanged();
    }

    public void onSingleChoiceClick(PremiumSingleChoiceSetting item) {
        mClickedItem = item;

        int value = getSelectionForSingleChoiceValue(item);

        AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

        builder.setTitle(item.getNameId());
        builder.setSingleChoiceItems(item.getChoicesId(), value, this);

        mDialog = builder.show();
    }

    public void onSingleChoiceClick(SingleChoiceSetting item) {
        mClickedItem = item;

        int value = getSelectionForSingleChoiceValue(item);

        AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

        builder.setTitle(item.getNameId());
        builder.setSingleChoiceItems(item.getChoicesId(), value, this);

        mDialog = builder.show();
    }

    public void onSingleChoiceClick(SingleChoiceSetting item, int position) {
        mClickedPosition = position;

        if (!item.isPremium() || MainActivity.isPremiumActive()) {
            // Setting is either not Premium, or the user has Premium
            onSingleChoiceClick(item);
            return;
        }

        // User needs Premium, invoke the billing flow
        MainActivity.invokePremiumBilling(() -> onSingleChoiceClick(item));
    }

    public void onSingleChoiceClick(PremiumSingleChoiceSetting item, int position) {
        mClickedPosition = position;

        if (!item.isPremium() || MainActivity.isPremiumActive()) {
            // Setting is either not Premium, or the user has Premium
            onSingleChoiceClick(item);
            return;
        }

        // User needs Premium, invoke the billing flow
        MainActivity.invokePremiumBilling(() -> onSingleChoiceClick(item));
    }

    public void onStringSingleChoiceClick(StringSingleChoiceSetting item) {
        mClickedItem = item;

        AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

        builder.setTitle(item.getNameId());
        builder.setSingleChoiceItems(item.getChoicesId(), item.getSelectValueIndex(), this);

        mDialog = builder.show();
    }

    public void onStringSingleChoiceClick(StringSingleChoiceSetting item, int position) {
        mClickedPosition = position;

        if (!item.isPremium() || MainActivity.isPremiumActive()) {
            // Setting is either not Premium, or the user has Premium
            onStringSingleChoiceClick(item);
            return;
        }

        // User needs Premium, invoke the billing flow
        MainActivity.invokePremiumBilling(() -> onStringSingleChoiceClick(item));
    }

    DialogInterface.OnClickListener defaultCancelListener = (dialog, which) -> closeDialog();

    public void onDateTimeClick(DateTimeSetting item, int position) {
        mClickedItem = item;
        mClickedPosition = position;

        AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

        LayoutInflater inflater = LayoutInflater.from(mView.getActivity());
        View view = inflater.inflate(R.layout.sysclock_datetime_picker, null);

        DatePicker dp = view.findViewById(R.id.date_picker);
        TimePicker tp = view.findViewById(R.id.time_picker);

        //set date and time to substrings of settingValue; format = 2018-12-24 04:20:69 (alright maybe not that 69)
        String settingValue = item.getValue();
        dp.updateDate(Integer.parseInt(settingValue.substring(0, 4)), Integer.parseInt(settingValue.substring(5, 7)) - 1, Integer.parseInt(settingValue.substring(8, 10)));

        tp.setIs24HourView(true);
        tp.setHour(Integer.parseInt(settingValue.substring(11, 13)));
        tp.setMinute(Integer.parseInt(settingValue.substring(14, 16)));

        DialogInterface.OnClickListener ok = (dialog, which) -> {
            //set it
            int year = dp.getYear();
            if (year < 2000) {
                year = 2000;
            }
            String month = ("00" + (dp.getMonth() + 1)).substring(String.valueOf(dp.getMonth() + 1).length());
            String day = ("00" + dp.getDayOfMonth()).substring(String.valueOf(dp.getDayOfMonth()).length());
            String hr = ("00" + tp.getHour()).substring(String.valueOf(tp.getHour()).length());
            String min = ("00" + tp.getMinute()).substring(String.valueOf(tp.getMinute()).length());
            String datetime = year + "-" + month + "-" + day + " " + hr + ":" + min + ":01";

            StringSetting setting = item.setSelectedValue(datetime);
            if (setting != null) {
                mView.putSetting(setting);
            }

            mView.onSettingChanged();

            mClickedItem = null;
            closeDialog();
        };

        builder.setView(view);
        builder.setPositiveButton(android.R.string.ok, ok);
        builder.setNegativeButton(android.R.string.cancel, defaultCancelListener);
        mDialog = builder.show();
    }

    public void onSliderClick(SliderSetting item, int position) {
        mClickedItem = item;
        mClickedPosition = position;
        mSeekbarProgress = item.getSelectedValue();
        AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

        LayoutInflater inflater = LayoutInflater.from(mView.getActivity());
        View view = inflater.inflate(R.layout.dialog_seekbar, null);

        SeekBar seekbar = view.findViewById(R.id.seekbar);

        builder.setTitle(item.getNameId());
        builder.setView(view);
        builder.setPositiveButton(android.R.string.ok, this);
        builder.setNegativeButton(android.R.string.cancel, defaultCancelListener);
        builder.setNeutralButton(R.string.slider_default, (DialogInterface dialog, int which) -> {
            seekbar.setProgress(item.getDefaultValue());
            onClick(dialog, which);
        });
        mDialog = builder.show();

        mTextSliderValue = view.findViewById(R.id.text_value);
        mTextSliderValue.setText(String.valueOf(mSeekbarProgress));

        TextView units = view.findViewById(R.id.text_units);
        units.setText(item.getUnits());

        seekbar.setMin(item.getMin());
        seekbar.setMax(item.getMax());
        seekbar.setProgress(mSeekbarProgress);

        seekbar.setOnSeekBarChangeListener(this);
    }

    public void onSubmenuClick(SubmenuSetting item) {
        mView.loadSubMenu(item.getMenuKey());
    }

    public void onInputBindingClick(final InputBindingSetting item, final int position) {
        final MotionAlertDialog dialog = new MotionAlertDialog(mContext, item);
        dialog.setTitle(R.string.input_binding);

        int messageResId = R.string.input_binding_description;
        if (item.IsAxisMappingSupported() && !item.IsTrigger()) {
            // Use specialized message for axis left/right or up/down
            if (item.IsHorizontalOrientation()) {
                messageResId = R.string.input_binding_description_horizontal_axis;
            } else {
                messageResId = R.string.input_binding_description_vertical_axis;
            }
        }

        dialog.setMessage(String.format(mContext.getString(messageResId), mContext.getString(item.getNameId())));
        dialog.setButton(AlertDialog.BUTTON_NEGATIVE, mContext.getString(android.R.string.cancel), this);
        dialog.setButton(AlertDialog.BUTTON_NEUTRAL, mContext.getString(R.string.clear), (dialogInterface, i) ->
                item.removeOldMapping());
        dialog.setOnDismissListener(dialog1 ->
        {
            StringSetting setting = new StringSetting(item.getKey(), item.getSection(), item.getValue());
            notifyItemChanged(position);

            mView.putSetting(setting);

            mView.onSettingChanged();
        });
        dialog.setCanceledOnTouchOutside(false);
        dialog.show();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (mClickedItem instanceof SingleChoiceSetting) {
            SingleChoiceSetting scSetting = (SingleChoiceSetting) mClickedItem;

            int value = getValueForSingleChoiceSelection(scSetting, which);
            if (scSetting.getSelectedValue() != value) {
                mView.onSettingChanged();
            }

            // Get the backing Setting, which may be null (if for example it was missing from the file)
            IntSetting setting = scSetting.setSelectedValue(value);
            if (setting != null) {
                mView.putSetting(setting);
            }

            closeDialog();
        } else if (mClickedItem instanceof PremiumSingleChoiceSetting) {
            PremiumSingleChoiceSetting scSetting = (PremiumSingleChoiceSetting) mClickedItem;
            scSetting.setSelectedValue(getValueForSingleChoiceSelection(scSetting, which));
            closeDialog();
        } else if (mClickedItem instanceof StringSingleChoiceSetting) {
            StringSingleChoiceSetting scSetting = (StringSingleChoiceSetting) mClickedItem;
            String value = scSetting.getValueAt(which);
            if (!scSetting.getSelectedValue().equals(value))
                mView.onSettingChanged();

            StringSetting setting = scSetting.setSelectedValue(value);
            if (setting != null) {
                mView.putSetting(setting);
            }

            closeDialog();
        } else if (mClickedItem instanceof SliderSetting) {
            SliderSetting sliderSetting = (SliderSetting) mClickedItem;
            if (sliderSetting.getSelectedValue() != mSeekbarProgress) {
                mView.onSettingChanged();
            }

            if (sliderSetting.getSetting() instanceof FloatSetting) {
                float value = (float) mSeekbarProgress;

                FloatSetting setting = sliderSetting.setSelectedValue(value);
                if (setting != null) {
                    mView.putSetting(setting);
                }
            } else {
                IntSetting setting = sliderSetting.setSelectedValue(mSeekbarProgress);
                if (setting != null) {
                    mView.putSetting(setting);
                }
            }

            closeDialog();
        }

        mClickedItem = null;
        mSeekbarProgress = -1;
    }

    public void closeDialog() {
        if (mDialog != null) {
            if (mClickedPosition != -1) {
                notifyItemChanged(mClickedPosition);
                mClickedPosition = -1;
            }
            mDialog.dismiss();
            mDialog = null;
        }
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        mSeekbarProgress = progress;
        mTextSliderValue.setText(String.valueOf(mSeekbarProgress));
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
    }

    private int getValueForSingleChoiceSelection(SingleChoiceSetting item, int which) {
        int valuesId = item.getValuesId();

        if (valuesId > 0) {
            int[] valuesArray = mContext.getResources().getIntArray(valuesId);
            return valuesArray[which];
        } else {
            return which;
        }
    }

    private int getValueForSingleChoiceSelection(PremiumSingleChoiceSetting item, int which) {
        int valuesId = item.getValuesId();

        if (valuesId > 0) {
            int[] valuesArray = mContext.getResources().getIntArray(valuesId);
            return valuesArray[which];
        } else {
            return which;
        }
    }

    private int getSelectionForSingleChoiceValue(SingleChoiceSetting item) {
        int value = item.getSelectedValue();
        int valuesId = item.getValuesId();

        if (valuesId > 0) {
            int[] valuesArray = mContext.getResources().getIntArray(valuesId);
            for (int index = 0; index < valuesArray.length; index++) {
                int current = valuesArray[index];
                if (current == value) {
                    return index;
                }
            }
        } else {
            return value;
        }

        return -1;
    }

    private int getSelectionForSingleChoiceValue(PremiumSingleChoiceSetting item) {
        int value = item.getSelectedValue();
        int valuesId = item.getValuesId();

        if (valuesId > 0) {
            int[] valuesArray = mContext.getResources().getIntArray(valuesId);
            for (int index = 0; index < valuesArray.length; index++) {
                int current = valuesArray[index];
                if (current == value) {
                    return index;
                }
            }
        } else {
            return value;
        }

        return -1;
    }
}
