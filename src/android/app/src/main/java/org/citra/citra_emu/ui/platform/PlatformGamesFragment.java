package org.citra.citra_emu.ui.platform;


import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.database.Cursor;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Lifecycle;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.divider.MaterialDividerItemDecoration;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.adapters.GameAdapter;
import org.citra.citra_emu.model.GameDatabase;

public final class PlatformGamesFragment extends Fragment implements PlatformGamesView {
    private final PlatformGamesPresenter mPresenter = new PlatformGamesPresenter(this);

    private GameAdapter mAdapter;
    private RecyclerView mRecyclerView;
    private TextView mTextView;
    private SwipeRefreshLayout mPullToRefresh;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_grid, container, false);

        findViews(rootView);

        mPresenter.onCreateView();

        return rootView;
    }

    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private void onPullToRefresh() {
        Runnable onPostRunnable = () -> {
            updateTextView();
            mPullToRefresh.setRefreshing(false);
        };
        Runnable scanLibraryRunnable = () -> {
            GameDatabase databaseHelper = CitraApplication.databaseHelper;
            databaseHelper.scanLibrary(databaseHelper.getWritableDatabase());
            mPresenter.refresh();
            mHandler.post(onPostRunnable);
        };

        mExecutor.execute(scanLibraryRunnable);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        int columns = getResources().getInteger(R.integer.game_grid_columns);
        RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getActivity(), columns);
        mAdapter = new GameAdapter();

        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.setAdapter(mAdapter);
        MaterialDividerItemDecoration divider = new MaterialDividerItemDecoration(requireContext(), LinearLayoutManager.VERTICAL);
        divider.setLastItemDecorated(false);
        mRecyclerView.addItemDecoration(divider);

        // Add swipe down to refresh gesture
        mPullToRefresh.setOnRefreshListener(this::onPullToRefresh);
        mPullToRefresh.setProgressBackgroundColorSchemeColor(MaterialColors.getColor(mPullToRefresh, R.attr.colorPrimary));
        mPullToRefresh.setColorSchemeColors(MaterialColors.getColor(mPullToRefresh, R.attr.colorOnPrimary));

        setInsets();
    }

    @Override
    public void refresh() {
        mPresenter.refresh();
        updateTextView();
    }

    @Override
    public void showGames(Cursor games) {
        if (mAdapter != null) {
            mAdapter.swapCursor(games);
        }
        updateTextView();
    }

    private void updateTextView() {
        mTextView.setVisibility(mAdapter.getItemCount() == 0 ? View.VISIBLE : View.GONE);
    }

    private void findViews(View root) {
        mRecyclerView = root.findViewById(R.id.grid_games);
        mTextView = root.findViewById(R.id.gamelist_empty_text);
        mPullToRefresh = root.findViewById(R.id.refresh_grid_games);
    }

    private void setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(mRecyclerView, (v, windowInsets) -> {
            Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(0, 0, 0, insets.bottom);
            return windowInsets;
        });
    }
}
