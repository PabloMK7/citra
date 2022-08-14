package org.citra.citra_emu.features.cheats.model;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

public class CheatsViewModel extends ViewModel {
    private int mSelectedCheatPosition = -1;
    private final MutableLiveData<Cheat> mSelectedCheat = new MutableLiveData<>(null);
    private final MutableLiveData<Boolean> mIsAdding = new MutableLiveData<>(false);
    private final MutableLiveData<Boolean> mIsEditing = new MutableLiveData<>(false);

    private final MutableLiveData<Integer> mCheatAddedEvent = new MutableLiveData<>(null);
    private final MutableLiveData<Integer> mCheatChangedEvent = new MutableLiveData<>(null);
    private final MutableLiveData<Integer> mCheatDeletedEvent = new MutableLiveData<>(null);
    private final MutableLiveData<Boolean> mOpenDetailsViewEvent = new MutableLiveData<>(false);

    private Cheat[] mCheats;
    private boolean mCheatsNeedSaving = false;

    public void load() {
        mCheats = CheatEngine.getCheats();

        for (int i = 0; i < mCheats.length; i++) {
            int position = i;
            mCheats[i].setEnabledChangedCallback(() -> {
                mCheatsNeedSaving = true;
                notifyCheatUpdated(position);
            });
        }
    }

    public void saveIfNeeded() {
        if (mCheatsNeedSaving) {
            CheatEngine.saveCheatFile();
            mCheatsNeedSaving = false;
        }
    }

    public Cheat[] getCheats() {
        return mCheats;
    }

    public LiveData<Cheat> getSelectedCheat() {
        return mSelectedCheat;
    }

    public void setSelectedCheat(Cheat cheat, int position) {
        if (mIsEditing.getValue()) {
            setIsEditing(false);
        }

        mSelectedCheat.setValue(cheat);
        mSelectedCheatPosition = position;
    }

    public LiveData<Boolean> getIsAdding() {
        return mIsAdding;
    }

    public LiveData<Boolean> getIsEditing() {
        return mIsEditing;
    }

    public void setIsEditing(boolean isEditing) {
        mIsEditing.setValue(isEditing);

        if (mIsAdding.getValue() && !isEditing) {
            mIsAdding.setValue(false);
            setSelectedCheat(null, -1);
        }
    }

    /**
     * When a cheat is added, the integer stored in the returned LiveData
     * changes to the position of that cheat, then changes back to null.
     */
    public LiveData<Integer> getCheatAddedEvent() {
        return mCheatAddedEvent;
    }

    private void notifyCheatAdded(int position) {
        mCheatAddedEvent.setValue(position);
        mCheatAddedEvent.setValue(null);
    }

    public void startAddingCheat() {
        mSelectedCheat.setValue(null);
        mSelectedCheatPosition = -1;

        mIsAdding.setValue(true);
        mIsEditing.setValue(true);
    }

    public void finishAddingCheat(Cheat cheat) {
        if (!mIsAdding.getValue()) {
            throw new IllegalStateException();
        }

        mIsAdding.setValue(false);
        mIsEditing.setValue(false);

        int position = mCheats.length;

        CheatEngine.addCheat(cheat);

        mCheatsNeedSaving = true;
        load();

        notifyCheatAdded(position);
        setSelectedCheat(mCheats[position], position);
    }

    /**
     * When a cheat is edited, the integer stored in the returned LiveData
     * changes to the position of that cheat, then changes back to null.
     */
    public LiveData<Integer> getCheatUpdatedEvent() {
        return mCheatChangedEvent;
    }

    /**
     * Notifies that an edit has been made to the contents of the cheat at the given position.
     */
    private void notifyCheatUpdated(int position) {
        mCheatChangedEvent.setValue(position);
        mCheatChangedEvent.setValue(null);
    }

    public void updateSelectedCheat(Cheat newCheat) {
        CheatEngine.updateCheat(mSelectedCheatPosition, newCheat);

        mCheatsNeedSaving = true;
        load();

        notifyCheatUpdated(mSelectedCheatPosition);
        setSelectedCheat(mCheats[mSelectedCheatPosition], mSelectedCheatPosition);
    }

    /**
     * When a cheat is deleted, the integer stored in the returned LiveData
     * changes to the position of that cheat, then changes back to null.
     */
    public LiveData<Integer> getCheatDeletedEvent() {
        return mCheatDeletedEvent;
    }

    /**
     * Notifies that the cheat at the given position has been deleted.
     */
    private void notifyCheatDeleted(int position) {
        mCheatDeletedEvent.setValue(position);
        mCheatDeletedEvent.setValue(null);
    }

    public void deleteSelectedCheat() {
        int position = mSelectedCheatPosition;

        setSelectedCheat(null, -1);

        CheatEngine.removeCheat(position);

        mCheatsNeedSaving = true;
        load();

        notifyCheatDeleted(position);
    }

    public LiveData<Boolean> getOpenDetailsViewEvent() {
        return mOpenDetailsViewEvent;
    }

    public void openDetailsView() {
        mOpenDetailsViewEvent.setValue(true);
        mOpenDetailsViewEvent.setValue(false);
    }
}
