package org.citra.citra_emu.utils;

import android.content.Context;
import android.content.res.Resources;
import android.view.ViewGroup;

import androidx.core.graphics.Insets;

import com.google.android.material.appbar.AppBarLayout;

public class InsetsHelper {
    public static final int THREE_BUTTON_NAVIGATION = 0;
    public static final int TWO_BUTTON_NAVIGATION = 1;
    public static final int GESTURE_NAVIGATION = 2;

    public static void insetAppBar(Insets insets, AppBarLayout appBarLayout)
    {
        ViewGroup.MarginLayoutParams mlpAppBar =
                (ViewGroup.MarginLayoutParams) appBarLayout.getLayoutParams();
        mlpAppBar.leftMargin = insets.left;
        mlpAppBar.rightMargin = insets.right;
        appBarLayout.setLayoutParams(mlpAppBar);
    }

    public static int getSystemGestureType(Context context) {
        Resources resources = context.getResources();
        int resourceId = resources.getIdentifier("config_navBarInteractionMode", "integer", "android");
        if (resourceId != 0) {
            return resources.getInteger(resourceId);
        }
        return 0;
    }
}
