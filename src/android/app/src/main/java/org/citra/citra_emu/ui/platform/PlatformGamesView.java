package org.citra.citra_emu.ui.platform;

import android.database.Cursor;

/**
 * Abstraction for a screen representing a single platform's games.
 */
public interface PlatformGamesView {
    /**
     * Tell the view to refresh its contents.
     */
    void refresh();

    /**
     * To be called when an asynchronous database read completes. Passes the
     * result, in this case a {@link Cursor}, to the view.
     *
     * @param games A Cursor containing the games read from the database.
     */
    void showGames(Cursor games);
}
