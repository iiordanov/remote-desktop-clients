/**
 * Copyright (C) 2023- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */
package com.iiordanov.bVNC.dialogs

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TableLayout
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import com.iiordanov.bVNC.App
import com.iiordanov.bVNC.Utils
import com.undatech.remoteClientUi.R
import java.io.IOException

class RateOrShareFragment : DialogFragment() {
    private var layout: TableLayout? = null
    private var donationButton: Button? = null
    private var previousVersionsButton: Button? = null
    private var versionAndCode: TextView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(TAG, "onCreate called")
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        Log.d(TAG, "onCreateView called")
        setTitle()
        val v = inflater.inflate(R.layout.rateorshare, container, false)
        layout = v.findViewById<View>(R.id.layout) as TableLayout
        donationButton = v.findViewById(R.id.buttonDonate)
        previousVersionsButton = v.findViewById(R.id.buttonPreviousVersions)
        if (!Utils.isFree(activity)) {
            donationButton?.visibility = View.GONE
        }
        if (Utils.isOpaque(activity)) {
            previousVersionsButton?.visibility = View.GONE
        }
        versionAndCode = v.findViewById<View>(R.id.versionAndCode) as TextView
        versionAndCode?.text = Utils.getVersionAndCode(v.context)
        setVisibilityOfElements(v)
        return v
    }

    fun setTitle() {
        var titleString = getString(R.string.action_rate_or_share_app)
        if (Utils.isCustom(context)){
            try {
                titleString = getString(
                    requireContext().resources.getIdentifier(
                        Utils.getStringConfigAttribute(App.configFileReader.configData, TAG.replaceFirstChar { it.lowercase() }, "title", "key"),
                        "string",
                        Utils.pName(context)
                    )
                )
            }
            catch (e: NullPointerException) {
                isCustomNullPointerException(e)
            }
        }
        dialog?.setTitle(titleString)
    }

    fun setVisibilityOfElements(v: View) {
        if (Utils.isCustom(context)){
            try {
                Utils.setVisibilityForViewElementsViaConfig(context, App.configFileReader.configData, TAG.replaceFirstChar { it.lowercase() }, v)
            }
            catch (e: NullPointerException) {
                isCustomNullPointerException(e)
            }
        }
    }

    fun isCustomNullPointerException(e: NullPointerException) {
        Log.e(TAG, "Error referencing attribute in config file.")
        Log.e(TAG, "Printing Stack Trace")
        e.printStackTrace()
    }

    companion object {
        var TAG = "RateOrShareFragment"
    }
}