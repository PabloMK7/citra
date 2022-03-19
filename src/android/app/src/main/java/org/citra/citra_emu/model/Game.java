package org.citra.citra_emu.model;

import android.content.ContentValues;
import android.database.Cursor;

import java.nio.file.Paths;

public final class Game {
    private String mTitle;
    private String mDescription;
    private String mPath;
    private String mGameId;
    private String mCompany;
    private String mRegions;

    public Game(String title, String description, String regions, String path,
                String gameId, String company) {
        mTitle = title;
        mDescription = description;
        mRegions = regions;
        mPath = path;
        mGameId = gameId;
        mCompany = company;
    }

    public static ContentValues asContentValues(String title, String description, String regions, String path, String gameId, String company) {
        ContentValues values = new ContentValues();

        if (gameId.isEmpty()) {
            // Homebrew, etc. may not have a game ID, use filename as a unique identifier
            gameId = Paths.get(path).getFileName().toString();
        }

        values.put(GameDatabase.KEY_GAME_TITLE, title);
        values.put(GameDatabase.KEY_GAME_DESCRIPTION, description);
        values.put(GameDatabase.KEY_GAME_REGIONS, regions);
        values.put(GameDatabase.KEY_GAME_PATH, path);
        values.put(GameDatabase.KEY_GAME_ID, gameId);
        values.put(GameDatabase.KEY_GAME_COMPANY, company);

        return values;
    }

    public static Game fromCursor(Cursor cursor) {
        return new Game(cursor.getString(GameDatabase.GAME_COLUMN_TITLE),
                cursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION),
                cursor.getString(GameDatabase.GAME_COLUMN_REGIONS),
                cursor.getString(GameDatabase.GAME_COLUMN_PATH),
                cursor.getString(GameDatabase.GAME_COLUMN_GAME_ID),
                cursor.getString(GameDatabase.GAME_COLUMN_COMPANY));
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDescription() {
        return mDescription;
    }

    public String getCompany() {
        return mCompany;
    }

    public String getRegions() {
        return mRegions;
    }

    public String getPath() {
        return mPath;
    }

    public String getGameId() {
        return mGameId;
    }
}
