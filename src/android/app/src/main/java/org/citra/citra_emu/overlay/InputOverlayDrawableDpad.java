/**
 * Copyright 2016 Dolphin Emulator Project
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
public final class InputOverlayDrawableDpad {
    public static final float VIRT_AXIS_DEADZONE = 0.5f;
    // The ID identifying what type of button this Drawable represents.
    private int mUpButtonId;
    private int mDownButtonId;
    private int mLeftButtonId;
    private int mRightButtonId;
    private int mTrackId;
    private int mPreviousTouchX, mPreviousTouchY;
    private int mControlPositionX, mControlPositionY;
    private int mWidth;
    private int mHeight;
    private BitmapDrawable mDefaultStateBitmap;
    private BitmapDrawable mPressedOneDirectionStateBitmap;
    private BitmapDrawable mPressedTwoDirectionsStateBitmap;
    private boolean mUpButtonState;
    private boolean mDownButtonState;
    private boolean mLeftButtonState;
    private boolean mRightButtonState;

    /**
     * Constructor
     *
     * @param res                             {@link Resources} instance.
     * @param defaultStateBitmap              {@link Bitmap} of the default state.
     * @param pressedOneDirectionStateBitmap  {@link Bitmap} of the pressed state in one direction.
     * @param pressedTwoDirectionsStateBitmap {@link Bitmap} of the pressed state in two direction.
     * @param buttonUp                        Identifier for the up button.
     * @param buttonDown                      Identifier for the down button.
     * @param buttonLeft                      Identifier for the left button.
     * @param buttonRight                     Identifier for the right button.
     */
    public InputOverlayDrawableDpad(Resources res,
                                    Bitmap defaultStateBitmap,
                                    Bitmap pressedOneDirectionStateBitmap,
                                    Bitmap pressedTwoDirectionsStateBitmap,
                                    int buttonUp, int buttonDown,
                                    int buttonLeft, int buttonRight) {
        mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
        mPressedOneDirectionStateBitmap = new BitmapDrawable(res, pressedOneDirectionStateBitmap);
        mPressedTwoDirectionsStateBitmap = new BitmapDrawable(res, pressedTwoDirectionsStateBitmap);

        mWidth = mDefaultStateBitmap.getIntrinsicWidth();
        mHeight = mDefaultStateBitmap.getIntrinsicHeight();

        mUpButtonId = buttonUp;
        mDownButtonId = buttonDown;
        mLeftButtonId = buttonLeft;
        mRightButtonId = buttonRight;

        mTrackId = -1;
    }

    public boolean updateStatus(MotionEvent event, boolean dpadSlide) {
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
            mTrackId = pointerId;
        }

        if (isActionUp) {
            if (mTrackId != pointerId) {
                return false;
            }
            mTrackId = -1;
            mUpButtonState = false;
            mDownButtonState = false;
            mLeftButtonState = false;
            mRightButtonState = false;
            return true;
        }

        if (mTrackId == -1) {
            return false;
        }

        if (!dpadSlide && !isActionDown) {
            return false;
        }

        for (int i = 0; i < event.getPointerCount(); i++) {
            if (mTrackId != event.getPointerId(i)) {
                continue;
            }
            float touchX = event.getX(i);
            float touchY = event.getY(i);
            float maxY = getBounds().bottom;
            float maxX = getBounds().right;
            touchX -= getBounds().centerX();
            maxX -= getBounds().centerX();
            touchY -= getBounds().centerY();
            maxY -= getBounds().centerY();
            final float AxisX = touchX / maxX;
            final float AxisY = touchY / maxY;
            final boolean upState = mUpButtonState;
            final boolean downState = mDownButtonState;
            final boolean leftState = mLeftButtonState;
            final boolean rightState = mRightButtonState;

            mUpButtonState = AxisY < -InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE;
            mDownButtonState = AxisY > InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE;
            mLeftButtonState = AxisX < -InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE;
            mRightButtonState = AxisX > InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE;
            return upState != mUpButtonState || downState != mDownButtonState || leftState != mLeftButtonState || rightState != mRightButtonState;
        }

        return false;
    }

    public void draw(Canvas canvas) {
        int px = mControlPositionX + (getWidth() / 2);
        int py = mControlPositionY + (getHeight() / 2);

        // Pressed up
        if (mUpButtonState && !mLeftButtonState && !mRightButtonState) {
            mPressedOneDirectionStateBitmap.draw(canvas);
            return;
        }

        // Pressed down
        if (mDownButtonState && !mLeftButtonState && !mRightButtonState) {
            canvas.save();
            canvas.rotate(180, px, py);
            mPressedOneDirectionStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Pressed left
        if (mLeftButtonState && !mUpButtonState && !mDownButtonState) {
            canvas.save();
            canvas.rotate(270, px, py);
            mPressedOneDirectionStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Pressed right
        if (mRightButtonState && !mUpButtonState && !mDownButtonState) {
            canvas.save();
            canvas.rotate(90, px, py);
            mPressedOneDirectionStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Pressed up left
        if (mUpButtonState && mLeftButtonState && !mRightButtonState) {
            mPressedTwoDirectionsStateBitmap.draw(canvas);
            return;
        }

        // Pressed up right
        if (mUpButtonState && !mLeftButtonState && mRightButtonState) {
            canvas.save();
            canvas.rotate(90, px, py);
            mPressedTwoDirectionsStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Pressed down left
        if (mDownButtonState && mLeftButtonState && !mRightButtonState) {
            canvas.save();
            canvas.rotate(270, px, py);
            mPressedTwoDirectionsStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Pressed down right
        if (mDownButtonState && !mLeftButtonState && mRightButtonState) {
            canvas.save();
            canvas.rotate(180, px, py);
            mPressedTwoDirectionsStateBitmap.draw(canvas);
            canvas.restore();
            return;
        }

        // Not pressed
        mDefaultStateBitmap.draw(canvas);
    }

    public int getUpId() {
        return mUpButtonId;
    }

    public int getDownId() {
        return mDownButtonId;
    }

    public int getLeftId() {
        return mLeftButtonId;
    }

    public int getRightId() {
        return mRightButtonId;
    }

    public int getTrackId() {
        return mTrackId;
    }

    public void setTrackId(int trackId) {
        mTrackId = trackId;
    }

    public int getUpStatus() {
        return mUpButtonState ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED;
    }

    public int getDownStatus() {
        return mDownButtonState ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED;
    }

    public int getLeftStatus() {
        return mLeftButtonState ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED;
    }

    public int getRightStatus() {
        return mRightButtonState ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED;
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

    public void setBounds(int left, int top, int right, int bottom) {
        mDefaultStateBitmap.setBounds(left, top, right, bottom);
        mPressedOneDirectionStateBitmap.setBounds(left, top, right, bottom);
        mPressedTwoDirectionsStateBitmap.setBounds(left, top, right, bottom);
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

}
