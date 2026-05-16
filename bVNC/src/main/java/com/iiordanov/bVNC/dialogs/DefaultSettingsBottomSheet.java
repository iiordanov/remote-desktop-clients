/**
 * Copyright (C) 2026- Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC.dialogs;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.bottomsheet.BottomSheetDialogFragment;
import com.undatech.remoteClientUi.R;

/**
 * Bottom sheet that lets the user choose which default-settings area to edit:
 * the app-wide global preferences, or the default connection template that new
 * and file-initiated connections start from.
 */
public class DefaultSettingsBottomSheet extends BottomSheetDialogFragment {

    public interface Callback {
        void onGlobalDefaultSettings();

        void onDefaultConnectionSettings();
    }

    private Callback callback;

    /**
     * Required public no-arg constructor: FragmentManager reflectively
     * re-instantiates the fragment after process death / configuration change.
     * Production code should use {@link #newInstance(Callback)}.
     */
    public DefaultSettingsBottomSheet() { }

    public static DefaultSettingsBottomSheet newInstance(Callback callback) {
        DefaultSettingsBottomSheet sheet = new DefaultSettingsBottomSheet();
        sheet.callback = callback;
        return sheet;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // The callback closes over the host Activity and is not part of saved
        // state, so a restored sheet cannot deliver its result. Dismiss it;
        // the user can reopen the menu.
        if (savedInstanceState != null && callback == null) {
            dismissAllowingStateLoss();
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.default_settings_bottom_sheet, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        expandSheetFully();
        view.findViewById(R.id.globalDefaultSettings).setOnClickListener(v -> {
            dismiss();
            callback.onGlobalDefaultSettings();
        });
        view.findViewById(R.id.defaultConnectionSettings).setOnClickListener(v -> {
            dismiss();
            callback.onDefaultConnectionSettings();
        });
    }

    private void expandSheetFully() {
        if (getDialog() instanceof BottomSheetDialog) {
            BottomSheetBehavior<?> behavior = ((BottomSheetDialog) getDialog()).getBehavior();
            behavior.setState(BottomSheetBehavior.STATE_EXPANDED);
            behavior.setSkipCollapsed(true);
        }
    }
}
