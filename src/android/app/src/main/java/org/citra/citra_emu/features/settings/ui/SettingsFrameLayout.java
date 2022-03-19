package org.citra.citra_emu.features.settings.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

/**
 * FrameLayout subclass with few Properties added to simplify animations.
 * Don't remove the methods appearing as unused, in order not to break the menu animations
 */
public final class SettingsFrameLayout extends FrameLayout {
    private float mVisibleness = 1.0f;

    public SettingsFrameLayout(Context context) {
        super(context);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    public float getYFraction() {
        return getY() / getHeight();
    }

    public void setYFraction(float yFraction) {
        final int height = getHeight();
        setY((height > 0) ? (yFraction * height) : -9999);
    }

    public float getVisibleness() {
        return mVisibleness;
    }

    public void setVisibleness(float visibleness) {
        setScaleX(visibleness);
        setScaleY(visibleness);
        setAlpha(visibleness);
    }
}
