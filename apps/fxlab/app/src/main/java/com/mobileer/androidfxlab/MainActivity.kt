/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.androidfxlab

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.SubMenu
import android.view.WindowManager
import android.widget.PopupMenu
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.mobileer.androidfxlab.R
import com.mobileer.androidfxlab.datatype.Effect

class MainActivity : AppCompatActivity() {

    private lateinit var recyclerView: RecyclerView

    private lateinit var addButton: FloatingActionButton


    val MY_PERMISSIONS_RECORD_AUDIO = 17

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.RECORD_AUDIO),
                MY_PERMISSIONS_RECORD_AUDIO
            )
        }

        recyclerView =
            findViewById<RecyclerView>(R.id.my_recycler_view).apply { adapter =
                EffectsAdapter
            }
        recyclerView.layoutManager = LinearLayoutManager(this)

        addButton = findViewById(R.id.floating_add_button)
        addButton.setOnClickListener { view ->
            val popup = PopupMenu(this, view)
            popup.menuInflater.inflate(R.menu.add_menu, popup.menu)
            val menuMap = HashMap<String, SubMenu>()
            for (effectName in NativeInterface.effectDescriptionMap.keys) {
                val cat = NativeInterface.effectDescriptionMap.getValue(effectName).category
                if (cat == "None") {
                    popup.menu.add(effectName)
                } else {
                    val subMenu = menuMap[cat] ?: popup.menu.addSubMenu(cat)
                    subMenu.add(effectName)
                    menuMap[cat] = subMenu
                }
            }
            popup.setOnMenuItemClickListener { menuItem ->
                NativeInterface.effectDescriptionMap[menuItem.title]?.let {
                    val toAdd = Effect(it)
                    EffectsAdapter.effectList.add(toAdd)
                    NativeInterface.addEffect(toAdd)
                    EffectsAdapter.notifyItemInserted(EffectsAdapter.effectList.size - 1)
                    true
                }
                false
            }
            popup.show()
        }
    }

    override fun onResume() {
        super.onResume()
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            == PackageManager.PERMISSION_GRANTED
        ) {
            NativeInterface.createAudioEngine()
        }
    }

    override fun onPause() {
        NativeInterface.destroyAudioEngine()
        EffectsAdapter.effectList.clear()
        EffectsAdapter.notifyDataSetChanged()
        super.onPause()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            MY_PERMISSIONS_RECORD_AUDIO -> {
                if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                    NativeInterface.createAudioEngine()
                } else {
                    val builder = AlertDialog.Builder(this).apply {
                        setMessage(
                            "Audio effects require audio input permissions! \n" +
                                    "Enable permissions and restart app to use."
                        )
                        setTitle("Permission Error")
                    }
                    builder.create().show()
                }
                return
            }
        }
    }
}
