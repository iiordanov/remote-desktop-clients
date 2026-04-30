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
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.bottomsheet.BottomSheetDialogFragment;
import com.google.android.material.button.MaterialButton;
import com.undatech.remoteClientUi.R;

/**
 * Bottom sheet that discovers remote desktop servers on the local network and lets the user
 * select one to pre-fill the connection setup form, or proceed to manual entry.
 * <p>
 * mDNS (Bonjour) starts automatically for protocols that support it (VNC, RDP, SPICE).
 * TCP port scanning is initiated manually by the user via the "Scan network" button.
 */
public class DiscoveryBottomSheet extends BottomSheetDialogFragment {

    public interface Callback {
        void onServerSelected(NetworkDiscovery.DiscoveredServer server);

        void onEnterManually();
    }

    private final String serviceType;
    private final Callback callback;

    private NetworkDiscovery discovery;
    private PortScanDiscovery portScanDiscovery;
    private DiscoveredServerAdapter adapter;
    private TextView emptyDiscovery;
    private TextView scanningStatus;
    private ProgressBar scanningSpinner;
    private Button scanButton;

    public DiscoveryBottomSheet(String serviceType, Callback callback) {
        this.serviceType = serviceType;
        this.callback = callback;
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.discovery_bottom_sheet, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        expandSheetFully();
        emptyDiscovery = view.findViewById(R.id.emptyDiscovery);
        scanningStatus = view.findViewById(R.id.scanningStatus);
        scanningSpinner = view.findViewById(R.id.scanningSpinner);
        scanButton = view.findViewById(R.id.scanButton);
        scanButton.setOnClickListener(v -> startPortScan());

        adapter = new DiscoveredServerAdapter(server -> {
            dismiss();
            callback.onServerSelected(server);
        });

        RecyclerView recyclerView = view.findViewById(R.id.discoveryList);
        recyclerView.setLayoutManager(new LinearLayoutManager(requireContext()));
        recyclerView.setAdapter(adapter);

        MaterialButton enterManually = view.findViewById(R.id.enterManually);
        enterManually.setOnClickListener(v -> {
            dismiss();
            callback.onEnterManually();
        });

        // mDNS discovery starts automatically when the protocol supports it
        if (serviceType != null) {
            discovery = new NetworkDiscovery(requireContext(), serviceType,
                    server -> postToUi(() -> addDiscoveredServer(server)));
            discovery.start();
        }

    }

    /**
     * Expands the sheet fully so the RecyclerView and the "Enter manually" button are both
     * visible, and upward scrolls inside the list are consumed by the list rather than
     * being intercepted by BottomSheetBehavior as a collapse gesture.
     */
    private void expandSheetFully() {
        if (getDialog() instanceof BottomSheetDialog) {
            BottomSheetBehavior<?> behavior = ((BottomSheetDialog) getDialog()).getBehavior();
            behavior.setState(BottomSheetBehavior.STATE_EXPANDED);
            behavior.setSkipCollapsed(true);
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (discovery != null) discovery.stop();
        if (portScanDiscovery != null) portScanDiscovery.stop();
    }

    private void startPortScan() {
        scanButton.setEnabled(false);
        scanningSpinner.setVisibility(View.VISIBLE);
        scanningStatus.setVisibility(View.VISIBLE);
        scanningStatus.setText(R.string.discovery_scanning);
        if (portScanDiscovery != null) portScanDiscovery.stop();
        portScanDiscovery = new PortScanDiscovery();
        portScanDiscovery.start(NetworkDiscovery.portsForApp(requireContext()),
                new PortScanDiscovery.Listener() {
                    @Override
                    public void onServerFound(NetworkDiscovery.DiscoveredServer server) {
                        postToUi(() -> addDiscoveredServer(server));
                    }

                    @Override
                    public void onProgressUpdate(int scanned, int total) {
                        postToUi(() -> scanningStatus.setText(
                                getString(R.string.discovery_scanning_progress, scanned, total)));
                    }

                    @Override
                    public void onScanComplete() {
                        postToUi(() -> {
                            scanningSpinner.setVisibility(View.GONE);
                            scanningStatus.setText(R.string.discovery_scan_complete);
                            scanButton.setEnabled(true);
                            if (adapter.isEmpty()) emptyDiscovery.setVisibility(View.VISIBLE);
                        });
                    }
                });
    }

    private void addDiscoveredServer(NetworkDiscovery.DiscoveredServer server) {
        adapter.addServer(server);
        emptyDiscovery.setVisibility(View.GONE);
    }

    private void postToUi(Runnable r) {
        if (isAdded()) {
            requireActivity().runOnUiThread(r);
        }
    }
}
