/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.citra.citra_emu.overlay;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.NativeLibrary.ButtonState;
import org.citra.citra_emu.NativeLibrary.ButtonType;
import org.citra.citra_emu.R;
import org.citra.citra_emu.utils.EmulationMenuSettings;

import java.util.HashSet;
import java.util.Set;

/**
 * Draws the interactive input overlay on top of the
 * {@link SurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener {
    private final Set<InputOverlayDrawableButton> overlayButtons = new HashSet<>();
    private final Set<InputOverlayDrawableDpad> overlayDpads = new HashSet<>();
    private final Set<InputOverlayDrawableJoystick> overlayJoysticks = new HashSet<>();

    private boolean mIsInEditMode = false;
    private InputOverlayDrawableButton mButtonBeingConfigured;
    private InputOverlayDrawableDpad mDpadBeingConfigured;
    private InputOverlayDrawableJoystick mJoystickBeingConfigured;

    private SharedPreferences mPreferences;

    // Stores the ID of the pointer that interacted with the 3DS touchscreen.
    private int mTouchscreenPointerId = -1;

    /**
     * Constructor
     *
     * @param context The current {@link Context}.
     * @param attrs   {@link AttributeSet} for parsing XML attributes.
     */
    public InputOverlay(Context context, AttributeSet attrs) {
        super(context, attrs);

        mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        if (!mPreferences.getBoolean("OverlayInit", false)) {
            defaultOverlay();
        }

        // Reset 3ds touchscreen pointer ID
        mTouchscreenPointerId = -1;

        // Load the controls.
        refreshControls();

        // Set the on touch listener.
        setOnTouchListener(this);

        // Force draw
        setWillNotDraw(false);

        // Request focus for the overlay so it has priority on presses.
        requestFocus();
    }

    /**
     * Resizes a {@link Bitmap} by a given scale factor
     *
     * @param context The current {@link Context}
     * @param bitmap  The {@link Bitmap} to scale.
     * @param scale   The scale factor for the bitmap.
     * @return The scaled {@link Bitmap}
     */
    public static Bitmap resizeBitmap(Context context, Bitmap bitmap, float scale) {
        // Determine the button size based on the smaller screen dimension.
        // This makes sure the buttons are the same size in both portrait and landscape.
        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int minDimension = Math.min(dm.widthPixels, dm.heightPixels);

        return Bitmap.createScaledBitmap(bitmap,
                (int) (minDimension * scale),
                (int) (minDimension * scale),
                true);
    }

    /**
     * Initializes an InputOverlayDrawableButton, given by resId, with all of the
     * parameters set for it to be properly shown on the InputOverlay.
     * <p>
     * This works due to the way the X and Y coordinates are stored within
     * the {@link SharedPreferences}.
     * <p>
     * In the input overlay configuration menu,
     * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
     * the X and Y coordinates of the button at the END of its touch event
     * (when you remove your finger/stylus from the touchscreen) are then stored
     * within a SharedPreferences instance so that those values can be retrieved here.
     * <p>
     * This has a few benefits over the conventional way of storing the values
     * (ie. within the Citra ini file).
     * <ul>
     * <li>No native calls</li>
     * <li>Keeps Android-only values inside the Android environment</li>
     * </ul>
     * <p>
     * Technically no modifications should need to be performed on the returned
     * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
     * for Android to call the onDraw method.
     *
     * @param context      The current {@link Context}.
     * @param defaultResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Default State).
     * @param pressedResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Pressed State).
     * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
     * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
     */
    private static InputOverlayDrawableButton initializeOverlayButton(Context context,
                                                                      int defaultResId, int pressedResId, int buttonId, String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on button ID and user preference
        float scale;

        switch (buttonId) {
            case ButtonType.BUTTON_HOME:
            case ButtonType.BUTTON_START:
            case ButtonType.BUTTON_SELECT:
                scale = 0.08f;
                break;
            case ButtonType.TRIGGER_L:
            case ButtonType.TRIGGER_R:
            case ButtonType.BUTTON_ZL:
            case ButtonType.BUTTON_ZR:
                scale = 0.18f;
                break;
            default:
                scale = 0.11f;
                break;
        }

        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableButton.
        final Bitmap defaultStateBitmap =
                resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
        final Bitmap pressedStateBitmap =
                resizeBitmap(context, BitmapFactory.decodeResource(res, pressedResId), scale);
        final InputOverlayDrawableButton overlayDrawable =
                new InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId);

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        String xKey;
        String yKey;

        xKey = buttonId + orientation + "-X";
        yKey = buttonId + orientation + "-Y";

        int drawableX = (int) sPrefs.getFloat(xKey, 0f);
        int drawableY = (int) sPrefs.getFloat(yKey, 0f);

        int width = overlayDrawable.getWidth();
        int height = overlayDrawable.getHeight();

        // Now set the bounds for the InputOverlayDrawableButton.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    /**
     * Initializes an {@link InputOverlayDrawableDpad}
     *
     * @param context                   The current {@link Context}.
     * @param defaultResId              The {@link Bitmap} resource ID of the default sate.
     * @param pressedOneDirectionResId  The {@link Bitmap} resource ID of the pressed sate in one direction.
     * @param pressedTwoDirectionsResId The {@link Bitmap} resource ID of the pressed sate in two directions.
     * @param buttonUp                  Identifier for the up button.
     * @param buttonDown                Identifier for the down button.
     * @param buttonLeft                Identifier for the left button.
     * @param buttonRight               Identifier for the right button.
     * @return the initialized {@link InputOverlayDrawableDpad}
     */
    private static InputOverlayDrawableDpad initializeOverlayDpad(Context context,
                                                                  int defaultResId,
                                                                  int pressedOneDirectionResId,
                                                                  int pressedTwoDirectionsResId,
                                                                  int buttonUp,
                                                                  int buttonDown,
                                                                  int buttonLeft,
                                                                  int buttonRight,
                                                                  String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on button ID and user preference
        float scale = 0.22f;

        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableDpad.
        final Bitmap defaultStateBitmap =
                resizeBitmap(context, BitmapFactory.decodeResource(res, defaultResId), scale);
        final Bitmap pressedOneDirectionStateBitmap =
                resizeBitmap(context, BitmapFactory.decodeResource(res, pressedOneDirectionResId),
                        scale);
        final Bitmap pressedTwoDirectionsStateBitmap =
                resizeBitmap(context, BitmapFactory.decodeResource(res, pressedTwoDirectionsResId),
                        scale);
        final InputOverlayDrawableDpad overlayDrawable =
                new InputOverlayDrawableDpad(res, defaultStateBitmap,
                        pressedOneDirectionStateBitmap, pressedTwoDirectionsStateBitmap,
                        buttonUp, buttonDown, buttonLeft, buttonRight);

        // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
        // These were set in the input overlay configuration menu.
        int drawableX = (int) sPrefs.getFloat(buttonUp + orientation + "-X", 0f);
        int drawableY = (int) sPrefs.getFloat(buttonUp + orientation + "-Y", 0f);

        int width = overlayDrawable.getWidth();
        int height = overlayDrawable.getHeight();

        // Now set the bounds for the InputOverlayDrawableDpad.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    /**
     * Initializes an {@link InputOverlayDrawableJoystick}
     *
     * @param context         The current {@link Context}
     * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
     * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
     * @param pressedResInner Resource ID for the pressed inner image of the joystick.
     * @param joystick        Identifier for which joystick this is.
     * @return the initialized {@link InputOverlayDrawableJoystick}.
     */
    private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context,
                                                                          int resOuter, int defaultResInner, int pressedResInner, int joystick, String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on user preference
        float scale = 0.275f;
        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableJoystick.
        final Bitmap bitmapOuter =
                resizeBitmap(context, BitmapFactory.decodeResource(res, resOuter), scale);
        final Bitmap bitmapInnerDefault = BitmapFactory.decodeResource(res, defaultResInner);
        final Bitmap bitmapInnerPressed = BitmapFactory.decodeResource(res, pressedResInner);

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        int drawableX = (int) sPrefs.getFloat(joystick + orientation + "-X", 0f);
        int drawableY = (int) sPrefs.getFloat(joystick + orientation + "-Y", 0f);

        // Decide inner scale based on joystick ID
        float outerScale = 1.f;
        if (joystick == ButtonType.STICK_C) {
            outerScale = 2.f;
        }

        // Now set the bounds for the InputOverlayDrawableJoystick.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
        int outerSize = bitmapOuter.getWidth();
        Rect outerRect = new Rect(drawableX, drawableY, drawableX + (int) (outerSize / outerScale), drawableY + (int) (outerSize / outerScale));
        Rect innerRect = new Rect(0, 0, (int) (outerSize / outerScale), (int) (outerSize / outerScale));

        // Send the drawableId to the joystick so it can be referenced when saving control position.
        final InputOverlayDrawableJoystick overlayDrawable
                = new InputOverlayDrawableJoystick(res, bitmapOuter,
                bitmapInnerDefault, bitmapInnerPressed,
                outerRect, innerRect, joystick);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        for (InputOverlayDrawableButton button : overlayButtons) {
            button.draw(canvas);
        }

        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            dpad.draw(canvas);
        }

        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            joystick.draw(canvas);
        }
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (isInEditMode()) {
            return onTouchWhileEditing(event);
        }

        int pointerIndex = event.getActionIndex();

        if (mPreferences.getBoolean("isTouchEnabled", true)) {
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    if (NativeLibrary.onTouchEvent(event.getX(pointerIndex), event.getY(pointerIndex), true)) {
                        mTouchscreenPointerId = event.getPointerId(pointerIndex);
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    if (mTouchscreenPointerId == event.getPointerId(pointerIndex)) {
                        // We don't really care where the touch has been released. We only care whether it has been
                        // released or not.
                        NativeLibrary.onTouchEvent(0, 0, false);
                        mTouchscreenPointerId = -1;
                    }
                    break;
            }

            for (int i = 0; i < event.getPointerCount(); i++) {
                if (mTouchscreenPointerId == event.getPointerId(i)) {
                    NativeLibrary.onTouchMoved(event.getX(i), event.getY(i));
                }
            }
        }

        for (InputOverlayDrawableButton button : overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // If a pointer enters the bounds of a button, press that button.
                    if (button.getBounds()
                            .contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex))) {
                        button.setPressedState(true);
                        button.setTrackId(event.getPointerId(pointerIndex));
                        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(),
                                ButtonState.PRESSED);
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    // If a pointer ends, release the button it was pressing.
                    if (button.getTrackId() == event.getPointerId(pointerIndex)) {
                        button.setPressedState(false);
                        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, button.getId(),
                                ButtonState.RELEASED);
                    }
                    break;
            }
        }

        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // If a pointer enters the bounds of a button, press that button.
                    if (dpad.getBounds()
                            .contains((int) event.getX(pointerIndex), (int) event.getY(pointerIndex))) {
                        dpad.setTrackId(event.getPointerId(pointerIndex));
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    // If a pointer ends, release the buttons.
                    if (dpad.getTrackId() == event.getPointerId(pointerIndex)) {
                        for (int i = 0; i < 4; i++) {
                            dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT);
                            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(i),
                                    NativeLibrary.ButtonState.RELEASED);
                        }
                        dpad.setTrackId(-1);
                    }
                    break;
            }

            if (dpad.getTrackId() != -1) {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    if (dpad.getTrackId() == event.getPointerId(i)) {
                        float touchX = event.getX(i);
                        float touchY = event.getY(i);
                        float maxY = dpad.getBounds().bottom;
                        float maxX = dpad.getBounds().right;
                        touchX -= dpad.getBounds().centerX();
                        maxX -= dpad.getBounds().centerX();
                        touchY -= dpad.getBounds().centerY();
                        maxY -= dpad.getBounds().centerY();
                        final float AxisX = touchX / maxX;
                        final float AxisY = touchY / maxY;

                        boolean up = false;
                        boolean down = false;
                        boolean left = false;
                        boolean right = false;
                        if (EmulationMenuSettings.getDpadSlideEnable() ||
                                (event.getAction() & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_DOWN ||
                                (event.getAction() & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_POINTER_DOWN) {
                            if (AxisY < -InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE) {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(0),
                                        NativeLibrary.ButtonState.PRESSED);
                                up = true;
                            } else {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(0),
                                        NativeLibrary.ButtonState.RELEASED);
                            }
                            if (AxisY > InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE) {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(1),
                                        NativeLibrary.ButtonState.PRESSED);
                                down = true;
                            } else {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(1),
                                        NativeLibrary.ButtonState.RELEASED);
                            }
                            if (AxisX < -InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE) {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(2),
                                        NativeLibrary.ButtonState.PRESSED);
                                left = true;
                            } else {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(2),
                                        NativeLibrary.ButtonState.RELEASED);
                            }
                            if (AxisX > InputOverlayDrawableDpad.VIRT_AXIS_DEADZONE) {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(3),
                                        NativeLibrary.ButtonState.PRESSED);
                                right = true;
                            } else {
                                NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, dpad.getId(3),
                                        NativeLibrary.ButtonState.RELEASED);
                            }

                            // Set state
                            if (up) {
                                if (left)
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT);
                                else if (right)
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT);
                                else
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP);
                            } else if (down) {
                                if (left)
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT);
                                else if (right)
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT);
                                else
                                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN);
                            } else if (left) {
                                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_LEFT);
                            } else if (right) {
                                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_RIGHT);
                            } else {
                                dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT);
                            }
                        }
                    }
                }
            }
        }

        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            joystick.TrackEvent(event);
            int axisID = joystick.getId();
            float[] axises = joystick.getAxisValues();

            NativeLibrary
                    .onGamePadMoveEvent(NativeLibrary.TouchScreenDevice, axisID, axises[0], axises[1]);
        }

        invalidate();

        return true;
    }

    public boolean onTouchWhileEditing(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int fingerPositionX = (int) event.getX(pointerIndex);
        int fingerPositionY = (int) event.getY(pointerIndex);

        String orientation =
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT ?
                        "-Portrait" : "";

        // Maybe combine Button and Joystick as subclasses of the same parent?
        // Or maybe create an interface like IMoveableHUDControl?

        for (InputOverlayDrawableButton button : overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // If no button is being moved now, remember the currently touched button to move.
                    if (mButtonBeingConfigured == null &&
                            button.getBounds().contains(fingerPositionX, fingerPositionY)) {
                        mButtonBeingConfigured = button;
                        mButtonBeingConfigured.onConfigureTouch(event);
                    }
                    break;
                case MotionEvent.ACTION_MOVE:
                    if (mButtonBeingConfigured != null) {
                        mButtonBeingConfigured.onConfigureTouch(event);
                        invalidate();
                        return true;
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    if (mButtonBeingConfigured == button) {
                        // Persist button position by saving new place.
                        saveControlPosition(mButtonBeingConfigured.getId(),
                                mButtonBeingConfigured.getBounds().left,
                                mButtonBeingConfigured.getBounds().top, orientation);
                        mButtonBeingConfigured = null;
                    }
                    break;
            }
        }

        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // If no button is being moved now, remember the currently touched button to move.
                    if (mButtonBeingConfigured == null &&
                            dpad.getBounds().contains(fingerPositionX, fingerPositionY)) {
                        mDpadBeingConfigured = dpad;
                        mDpadBeingConfigured.onConfigureTouch(event);
                    }
                    break;
                case MotionEvent.ACTION_MOVE:
                    if (mDpadBeingConfigured != null) {
                        mDpadBeingConfigured.onConfigureTouch(event);
                        invalidate();
                        return true;
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    if (mDpadBeingConfigured == dpad) {
                        // Persist button position by saving new place.
                        saveControlPosition(mDpadBeingConfigured.getId(0),
                                mDpadBeingConfigured.getBounds().left, mDpadBeingConfigured.getBounds().top,
                                orientation);
                        mDpadBeingConfigured = null;
                    }
                    break;
            }
        }

        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    if (mJoystickBeingConfigured == null &&
                            joystick.getBounds().contains(fingerPositionX, fingerPositionY)) {
                        mJoystickBeingConfigured = joystick;
                        mJoystickBeingConfigured.onConfigureTouch(event);
                    }
                    break;
                case MotionEvent.ACTION_MOVE:
                    if (mJoystickBeingConfigured != null) {
                        mJoystickBeingConfigured.onConfigureTouch(event);
                        invalidate();
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    if (mJoystickBeingConfigured != null) {
                        saveControlPosition(mJoystickBeingConfigured.getId(),
                                mJoystickBeingConfigured.getBounds().left,
                                mJoystickBeingConfigured.getBounds().top, orientation);
                        mJoystickBeingConfigured = null;
                    }
                    break;
            }
        }

        return true;
    }

    private void setDpadState(InputOverlayDrawableDpad dpad, boolean up, boolean down, boolean left,
                              boolean right) {
        if (up) {
            if (left)
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT);
            else if (right)
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT);
            else
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP);
        } else if (down) {
            if (left)
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT);
            else if (right)
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT);
            else
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN);
        } else if (left) {
            dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_LEFT);
        } else if (right) {
            dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_RIGHT);
        }
    }

    private void addOverlayControls(String orientation) {
        if (mPreferences.getBoolean("buttonToggle0", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_a,
                    R.drawable.button_a_pressed, ButtonType.BUTTON_A, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle1", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_b,
                    R.drawable.button_b_pressed, ButtonType.BUTTON_B, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle2", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_x,
                    R.drawable.button_x_pressed, ButtonType.BUTTON_X, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle3", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_y,
                    R.drawable.button_y_pressed, ButtonType.BUTTON_Y, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle4", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_l,
                    R.drawable.button_l_pressed, ButtonType.TRIGGER_L, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle5", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_r,
                    R.drawable.button_r_pressed, ButtonType.TRIGGER_R, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle6", false)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_zl,
                    R.drawable.button_zl_pressed, ButtonType.BUTTON_ZL, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle7", false)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_zr,
                    R.drawable.button_zr_pressed, ButtonType.BUTTON_ZR, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle8", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_start,
                    R.drawable.button_start_pressed, ButtonType.BUTTON_START, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle9", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.button_select,
                    R.drawable.button_select_pressed, ButtonType.BUTTON_SELECT, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle10", true)) {
            overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.dpad,
                    R.drawable.dpad_pressed_one_direction,
                    R.drawable.dpad_pressed_two_directions,
                    ButtonType.DPAD_UP, ButtonType.DPAD_DOWN,
                    ButtonType.DPAD_LEFT, ButtonType.DPAD_RIGHT, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle11", true)) {
            overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.stick_main_range,
                    R.drawable.stick_main, R.drawable.stick_main_pressed,
                    ButtonType.STICK_LEFT, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle12", false)) {
            overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.stick_c_range,
                    R.drawable.stick_c, R.drawable.stick_c_pressed, ButtonType.STICK_C, orientation));
        }
    }

    public void refreshControls() {
        // Remove all the overlay buttons from the HashSet.
        overlayButtons.clear();
        overlayDpads.clear();
        overlayJoysticks.clear();

        String orientation =
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT ?
                        "-Portrait" : "";

        // Add all the enabled overlay items back to the HashSet.
        if (EmulationMenuSettings.getShowOverlay()) {
            addOverlayControls(orientation);
        }

        invalidate();
    }

    private void saveControlPosition(int sharedPrefsId, int x, int y, String orientation) {
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
        sPrefsEditor.putFloat(sharedPrefsId + orientation + "-X", x);
        sPrefsEditor.putFloat(sharedPrefsId + orientation + "-Y", y);
        sPrefsEditor.apply();
    }

    public void setIsInEditMode(boolean isInEditMode) {
        mIsInEditMode = isInEditMode;
    }

    private void defaultOverlay() {
        if (!mPreferences.getBoolean("OverlayInit", false)) {
            // It's possible that a user has created their overlay before this was added
            // Only change the overlay if the 'A' button is not in the upper corner.
            if (mPreferences.getFloat(ButtonType.BUTTON_A + "-X", 0f) == 0f) {
                defaultOverlayLandscape();
            }
            if (mPreferences.getFloat(ButtonType.BUTTON_A + "-Portrait" + "-X", 0f) == 0f) {
                defaultOverlayPortrait();
            }
        }

        SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
        sPrefsEditor.putBoolean("OverlayInit", true);
        sPrefsEditor.apply();
    }

    public void resetButtonPlacement() {
        boolean isLandscape =
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE;

        if (isLandscape) {
            defaultOverlayLandscape();
        } else {
            defaultOverlayPortrait();
        }

        refreshControls();
    }

    private void defaultOverlayLandscape() {
        SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
        // Get screen size
        Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
        DisplayMetrics outMetrics = new DisplayMetrics();
        display.getMetrics(outMetrics);
        float maxX = outMetrics.heightPixels;
        float maxY = outMetrics.widthPixels;
        // Height and width changes depending on orientation. Use the larger value for height.
        if (maxY > maxX) {
            float tmp = maxX;
            maxX = maxY;
            maxY = tmp;
        }
        Resources res = getResources();

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_A_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_A_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_B_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_B_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_X_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_X_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_Y_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_Y_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZL + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZL_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZL + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZL_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZR + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZR_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZR + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZR_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_UP_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_UP_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-X", (((float) res.getInteger(R.integer.N3DS_TRIGGER_L_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-Y", (((float) res.getInteger(R.integer.N3DS_TRIGGER_L_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-X", (((float) res.getInteger(R.integer.N3DS_TRIGGER_R_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-Y", (((float) res.getInteger(R.integer.N3DS_TRIGGER_R_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_START + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_START_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_START + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_START_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_SELECT + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_SELECT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_SELECT + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_SELECT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_HOME_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_HOME_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_C + "-X", (((float) res.getInteger(R.integer.N3DS_STICK_C_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_C + "-Y", (((float) res.getInteger(R.integer.N3DS_STICK_C_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_LEFT + "-X", (((float) res.getInteger(R.integer.N3DS_STICK_MAIN_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_LEFT + "-Y", (((float) res.getInteger(R.integer.N3DS_STICK_MAIN_Y) / 1000) * maxY));

        // We want to commit right away, otherwise the overlay could load before this is saved.
        sPrefsEditor.commit();
    }

    private void defaultOverlayPortrait() {
        SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
        // Get screen size
        Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
        DisplayMetrics outMetrics = new DisplayMetrics();
        display.getMetrics(outMetrics);
        float maxX = outMetrics.heightPixels;
        float maxY = outMetrics.widthPixels;
        // Height and width changes depending on orientation. Use the larger value for height.
        if (maxY < maxX) {
            float tmp = maxX;
            maxX = maxY;
            maxY = tmp;
        }
        Resources res = getResources();
        String portrait = "-Portrait";

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_A_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_A_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_B_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_B_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_X_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_X_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_Y_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_Y_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZL + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZL_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZL + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZL_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZR + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZR_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_ZR + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_ZR_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_UP_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_UP_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_TRIGGER_L_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_TRIGGER_L_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_TRIGGER_R_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_TRIGGER_R_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_START + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_START_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_START + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_START_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_SELECT + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_SELECT_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_SELECT + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_SELECT_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_BUTTON_HOME_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_BUTTON_HOME_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_C + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_STICK_C_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_C + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_STICK_C_PORTRAIT_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_LEFT + portrait + "-X", (((float) res.getInteger(R.integer.N3DS_STICK_MAIN_PORTRAIT_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_LEFT + portrait + "-Y", (((float) res.getInteger(R.integer.N3DS_STICK_MAIN_PORTRAIT_Y) / 1000) * maxY));

        // We want to commit right away, otherwise the overlay could load before this is saved.
        sPrefsEditor.commit();
    }

    public boolean isInEditMode() {
        return mIsInEditMode;
    }
}
