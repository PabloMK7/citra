package org.citra.citra_emu.features.settings.ui;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.Setting;
import org.citra.citra_emu.features.settings.model.Settings;
import org.citra.citra_emu.features.settings.model.view.SettingsItem;
import org.citra.citra_emu.ui.DividerItemDecoration;

import java.util.ArrayList;

public final class SettingsFragment extends Fragment implements SettingsFragmentView {
    private static final String ARGUMENT_MENU_TAG = "menu_tag";
    private static final String ARGUMENT_GAME_ID = "game_id";

    private SettingsFragmentPresenter mPresenter = new SettingsFragmentPresenter(this);
    private SettingsActivityView mActivity;

    private SettingsAdapter mAdapter;

    public static Fragment newInstance(String menuTag, String gameId) {
        SettingsFragment fragment = new SettingsFragment();

        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_MENU_TAG, menuTag);
        arguments.putString(ARGUMENT_GAME_ID, gameId);

        fragment.setArguments(arguments);
        return fragment;
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);

        mActivity = (SettingsActivityView) context;
        mPresenter.onAttach();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setRetainInstance(true);
        String menuTag = getArguments().getString(ARGUMENT_MENU_TAG);
        String gameId = getArguments().getString(ARGUMENT_GAME_ID);

        mAdapter = new SettingsAdapter(this, getActivity());

        mPresenter.onCreate(menuTag, gameId);
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_settings, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        LinearLayoutManager manager = new LinearLayoutManager(getActivity());

        RecyclerView recyclerView = view.findViewById(R.id.list_settings);

        recyclerView.setAdapter(mAdapter);
        recyclerView.setLayoutManager(manager);
        recyclerView.addItemDecoration(new DividerItemDecoration(getActivity(), null));

        SettingsActivityView activity = (SettingsActivityView) getActivity();

        mPresenter.onViewCreated(activity.getSettings());
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mActivity = null;

        if (mAdapter != null) {
            mAdapter.closeDialog();
        }
    }

    @Override
    public void onSettingsFileLoaded(Settings settings) {
        mPresenter.setSettings(settings);
    }

    @Override
    public void passSettingsToActivity(Settings settings) {
        if (mActivity != null) {
            mActivity.setSettings(settings);
        }
    }

    @Override
    public void showSettingsList(ArrayList<SettingsItem> settingsList) {
        mAdapter.setSettings(settingsList);
    }

    @Override
    public void loadDefaultSettings() {
        mPresenter.loadDefaultSettings();
    }

    @Override
    public void loadSubMenu(String menuKey) {
        mActivity.showSettingsFragment(menuKey, true, getArguments().getString(ARGUMENT_GAME_ID));
    }

    @Override
    public void showToastMessage(String message, boolean is_long) {
        mActivity.showToastMessage(message, is_long);
    }

    @Override
    public void putSetting(Setting setting) {
        mPresenter.putSetting(setting);
    }

    @Override
    public void onSettingChanged() {
        mActivity.onSettingChanged();
    }
}
