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

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.undatech.remoteClientUi.R;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class DiscoveredServerAdapter extends RecyclerView.Adapter<DiscoveredServerAdapter.ViewHolder> {

    public interface OnServerClickListener {
        void onServerClick(NetworkDiscovery.DiscoveredServer server);
    }

    private final List<NetworkDiscovery.DiscoveredServer> servers = new ArrayList<>();
    private final Set<String> serverKeys = new HashSet<>();
    private final OnServerClickListener clickListener;

    public DiscoveredServerAdapter(OnServerClickListener clickListener) {
        this.clickListener = clickListener;
    }

    public void addServer(NetworkDiscovery.DiscoveredServer server) {
        if (!serverKeys.add(serverKey(server.host(), server.port()))) return;
        servers.add(server);
        notifyItemInserted(servers.size() - 1);
    }

    public boolean isEmpty() {
        return servers.isEmpty();
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.discovery_item, parent, false);
        ViewHolder holder = new ViewHolder(view);
        view.setOnClickListener(v -> {
            int pos = holder.getBindingAdapterPosition();
            if (pos != RecyclerView.NO_ID) {
                clickListener.onServerClick(servers.get(pos));
            }
        });
        return holder;
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        NetworkDiscovery.DiscoveredServer server = servers.get(position);
        holder.nameView.setText(server.name());
        holder.addressView.setText(
                holder.itemView.getContext().getString(
                        R.string.discovery_item_address, server.host(), server.port()));
    }

    @Override
    public int getItemCount() {
        return servers.size();
    }

    private static String serverKey(String host, int port) {
        return host + ":" + port;
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        final TextView nameView;
        final TextView addressView;

        ViewHolder(View itemView) {
            super(itemView);
            nameView = itemView.findViewById(R.id.serverName);
            addressView = itemView.findViewById(R.id.serverAddress);
        }
    }
}
