/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.citra.citra_emu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;

import org.citra.citra_emu.NativeLibrary;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableButton {
    // The ID identifying what type of button this Drawable represents.
    private int mButtonType;
    private int mTrackId;
    private int mPreviousTouchX, mPreviousTouchY;
    private int mControlPositionX, mControlPositionY;
    private int mWidth;
    private int mHeight;
    private BitmapDrawable mDefaultStateBitmap;
    private BitmapDrawable mPressedStateBitmap;
    private boolean mPressedState = false;

    /**
     * Constructor
     *
     * @param res                {@link Resources} instance.
     * @param defaultStateBitmap {@link Bitmap} to use with the default state Drawable.
     * @param pressedStateBitmap {@link Bitmap} to use with the pressed state Drawable.
     * @param buttonType         Identifier for this type of button.
     */
    public InputOverlayDrawableButton(Resources res, Bitmap defaultStateBitmap,
                                      Bitmap pressedStateBitmap, int buttonType) {
        mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
        mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
        mButtonType = buttonType;
        mTrackId = -1;

        mWidth = mDefaultStateBitmap.getIntrinsicWidth();
        mHeight = mDefaultStateBitmap.getIntrinsicHeight();
    }

    /**
     * Updates button status based on the motion event.
     *
     * @return true if value was changed
     */
    public boolean updateStatus(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int xPosition = (int) event.getX(pointerIndex);
        int yPosition = (int) event.getY(pointerIndex);
        int pointerId = event.getPointerId(pointerIndex);
        int motionEvent = event.getAction() & MotionEvent.ACTION_MASK;
        boolean isActionDown = motionEvent == MotionEvent.ACTION_DOWN || motionEvent == MotionEvent.ACTION_POINTER_DOWN;
        boolean isActionUp = motionEvent == MotionEvent.ACTION_UP || motionEvent == MotionEvent.ACTION_POINTER_UP;

        if (isActionDown) {
            if (!getBounds().contains(xPosition, yPosition)) {
                return false;
            }
            mPressedState = true;
            mTrackId = pointerId;
            return true;
        }

        if (isActionUp) {
            if (mTrackId != pointerId) {
                return false;
            }
            mPressedState = false;
            mTrackId = -1;
            return true;
        }

        return false;
    }

    public boolean onConfigureTouch(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int fingerPositionX = (int) event.getX(pointerIndex);
        int fingerPositionY = (int) event.getY(pointerIndex);
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                mPreviousTouchX = fingerPositionX;
                mPreviousTouchY = fingerPositionY;
                break;
            case MotionEvent.ACTION_MOVE:
                mControlPositionX += fingerPositionX - mPreviousTouchX;
                mControlPositionY += fingerPositionY - mPreviousTouchY;
                setBounds(mControlPositionX, mControlPositionY, getWidth() + mControlPositionX,
                        getHeight() + mControlPositionY);
                mPreviousTouchX = fingerPositionX;
                mPreviousTouchY = fingerPositionY;
                break;

        }
        return true;
    }

    public void setPosition(int x, int y) {
        mControlPositionX = x;
        mControlPositionY = y;
    }

    public void draw(Canvas canvas) {
        getCurrentStateBitmapDrawable().draw(canvas);
    }

    private BitmapDrawable getCurrentStateBitmapDrawable() {
        return mPressedState ? mPressedStateBitmap : mDefaultStateBitmap;
    }

    public void setBounds(int left, int top, int right, int bottom) {
        mDefaultStateBitmap.setBounds(left, top, right, bottom);
        mPressedStateBitmap.setBounds(left, top, right, bottom);
    }

    public int getId() {
        return mButtonType;
    }

    public int getTrackId() {
        return mTrackId;
    }

    public void setTrackId(int trackId) {
        mTrackId = trackId;
    }

    public int getStatus() {
        return mPressedState ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED;
    }

    public Rect getBounds() {
        return mDefaultStateBitmap.getBounds();
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }

    public void setPressedState(boolean isPressed) {
        mPressedState = isPressed;
    }
}
