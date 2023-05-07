package org.citra.citra_emu.adapters;

import android.content.Context;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.os.Build;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.features.cheats.ui.CheatsActivity;
import org.citra.citra_emu.model.GameDatabase;
import org.citra.citra_emu.utils.FileUtil;
import org.citra.citra_emu.utils.Log;
import org.citra.citra_emu.utils.PicassoUtils;
import org.citra.citra_emu.viewholders.GameViewHolder;

import java.util.stream.Stream;

/**
 * This adapter gets its information from a database Cursor. This fact, paired with the usage of
 * ContentProviders and Loaders, allows for efficient display of a limited view into a (possibly)
 * large dataset.
 */
public final class GameAdapter extends RecyclerView.Adapter<GameViewHolder> {
    private Cursor mCursor;
    private GameDataSetObserver mObserver;

    private boolean mDatasetValid;
    private long mLastClickTime = 0;

    /**
     * Initializes the adapter's observer, which watches for changes to the dataset. The adapter will
     * display no data until a Cursor is supplied by a CursorLoader.
     */
    public GameAdapter() {
        mDatasetValid = false;
        mObserver = new GameDataSetObserver();
    }

    /**
     * Called by the LayoutManager when it is necessary to create a new view.
     *
     * @param parent   The RecyclerView (I think?) the created view will be thrown into.
     * @param viewType Not used here, but useful when more than one type of child will be used in the RecyclerView.
     * @return The created ViewHolder with references to all the child view's members.
     */
    @Override
    public GameViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        // Create a new view.
        View gameCard = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.card_game, parent, false);

        gameCard.setOnClickListener(this::onClick);
        gameCard.setOnLongClickListener(this::onLongClick);

        // Use that view to create a ViewHolder.
        return new GameViewHolder(gameCard);
    }

    /**
     * Called by the LayoutManager when a new view is not necessary because we can recycle
     * an existing one (for example, if a view just scrolled onto the screen from the bottom, we
     * can use the view that just scrolled off the top instead of inflating a new one.)
     *
     * @param holder   A ViewHolder representing the view we're recycling.
     * @param position The position of the 'new' view in the dataset.
     */
    @RequiresApi(api = Build.VERSION_CODES.O)
    @Override
    public void onBindViewHolder(@NonNull GameViewHolder holder, int position) {
        if (mDatasetValid) {
            if (mCursor.moveToPosition(position)) {
                PicassoUtils.loadGameIcon(holder.imageIcon,
                        mCursor.getString(GameDatabase.GAME_COLUMN_PATH));

                holder.textGameTitle.setText(mCursor.getString(GameDatabase.GAME_COLUMN_TITLE).replaceAll("[\\t\\n\\r]+", " "));
                holder.textCompany.setText(mCursor.getString(GameDatabase.GAME_COLUMN_COMPANY));

                String filepath = mCursor.getString(GameDatabase.GAME_COLUMN_PATH);
                String filename;
                if (FileUtil.isNativePath(filepath)) {
                    filename = CitraApplication.documentsTree.getFilename(filepath);
                } else {
                    filename = FileUtil.getFilename(CitraApplication.getAppContext(), filepath);
                }
                holder.textFileName.setText(filename);

                // TODO These shouldn't be necessary once the move to a DB-based model is complete.
                holder.gameId = mCursor.getString(GameDatabase.GAME_COLUMN_GAME_ID);
                holder.path = mCursor.getString(GameDatabase.GAME_COLUMN_PATH);
                holder.title = mCursor.getString(GameDatabase.GAME_COLUMN_TITLE);
                holder.description = mCursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION);
                holder.regions = mCursor.getString(GameDatabase.GAME_COLUMN_REGIONS);
                holder.company = mCursor.getString(GameDatabase.GAME_COLUMN_COMPANY);

                final int backgroundColorId = isValidGame(holder.path) ? R.attr.colorSurface : R.attr.colorErrorContainer;
                View itemView = holder.getItemView();
                itemView.setBackgroundColor(MaterialColors.getColor(itemView, backgroundColorId));
            } else {
                Log.error("[GameAdapter] Can't bind view; Cursor is not valid.");
            }
        } else {
            Log.error("[GameAdapter] Can't bind view; dataset is not valid.");
        }
    }

    /**
     * Called by the LayoutManager to find out how much data we have.
     *
     * @return Size of the dataset.
     */
    @Override
    public int getItemCount() {
        if (mDatasetValid && mCursor != null) {
            return mCursor.getCount();
        }
        Log.error("[GameAdapter] Dataset is not valid.");
        return 0;
    }

    /**
     * Return the contents of the _id column for a given row.
     *
     * @param position The row for which Android wants an ID.
     * @return A valid ID from the database, or 0 if not available.
     */
    @Override
    public long getItemId(int position) {
        if (mDatasetValid && mCursor != null) {
            if (mCursor.moveToPosition(position)) {
                return mCursor.getLong(GameDatabase.COLUMN_DB_ID);
            }
        }

        Log.error("[GameAdapter] Dataset is not valid.");
        return 0;
    }

    /**
     * Tell Android whether or not each item in the dataset has a stable identifier.
     * Which it does, because it's a database, so always tell Android 'true'.
     *
     * @param hasStableIds ignored.
     */
    @Override
    public void setHasStableIds(boolean hasStableIds) {
        super.setHasStableIds(true);
    }

    /**
     * When a load is finished, call this to replace the existing data with the newly-loaded
     * data.
     *
     * @param cursor The newly-loaded Cursor.
     */
    public void swapCursor(Cursor cursor) {
        // Sanity check.
        if (cursor == mCursor) {
            return;
        }

        // Before getting rid of the old cursor, disassociate it from the Observer.
        final Cursor oldCursor = mCursor;
        if (oldCursor != null && mObserver != null) {
            oldCursor.unregisterDataSetObserver(mObserver);
        }

        mCursor = cursor;
        if (mCursor != null) {
            // Attempt to associate the new Cursor with the Observer.
            if (mObserver != null) {
                mCursor.registerDataSetObserver(mObserver);
            }

            mDatasetValid = true;
        } else {
            mDatasetValid = false;
        }

        notifyDataSetChanged();
    }

    /**
     * Launches the game that was clicked on.
     *
     * @param view The view representing the game the user wants to play.
     */
    private void onClick(View view) {
        // Double-click prevention, using threshold of 1000 ms
        if (SystemClock.elapsedRealtime() - mLastClickTime < 1000) {
            return;
        }
        mLastClickTime = SystemClock.elapsedRealtime();

        GameViewHolder holder = (GameViewHolder) view.getTag();

        EmulationActivity.launch((FragmentActivity) view.getContext(), holder.path, holder.title);
    }

    /**
     * Opens the cheats settings for the game that was clicked on.
     *
     * @param view The view representing the game the user wants to play.
     */
    private boolean onLongClick(View view) {
        Context context = view.getContext();
        GameViewHolder holder = (GameViewHolder) view.getTag();

        final long titleId = NativeLibrary.GetTitleId(holder.path);

        if (titleId == 0) {
            new MaterialAlertDialogBuilder(context)
                    .setIcon(R.mipmap.ic_launcher)
                    .setTitle(R.string.properties)
                    .setMessage(R.string.properties_not_loaded)
                    .setPositiveButton(android.R.string.ok, null)
                    .show();
        } else {
            CheatsActivity.launch(context, titleId);
        }

        return true;
    }

    private boolean isValidGame(String path) {
        return Stream.of(
                ".rar", ".zip", ".7z", ".torrent", ".tar", ".gz").noneMatch(suffix -> path.toLowerCase().endsWith(suffix));
    }

    private final class GameDataSetObserver extends DataSetObserver {
        @Override
        public void onChanged() {
            super.onChanged();

            mDatasetValid = true;
            notifyDataSetChanged();
        }

        @Override
        public void onInvalidated() {
            super.onInvalidated();

            mDatasetValid = false;
            notifyDataSetChanged();
        }
    }
}
