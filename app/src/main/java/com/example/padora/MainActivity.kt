package com.example.padora

import android.os.Bundle
import android.widget.TextView
import androidx.activity.ComponentActivity
import com.google.android.material.switchmaterial.SwitchMaterial
import com.google.firebase.FirebaseApp
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener

class MainActivity : ComponentActivity() {

    private lateinit var auth: FirebaseAuth
    private val db by lazy {
        FirebaseDatabase.getInstance(
            "https://iot-esp32-61821-default-rtdb.asia-southeast1.firebasedatabase.app"
        )
    }

    private lateinit var tvTemp: TextView
    private lateinit var tvHumid: TextView
    private lateinit var tvStatus: TextView
    private lateinit var swRL1: SwitchMaterial
    private lateinit var swRL2: SwitchMaterial
    private lateinit var swRL3: SwitchMaterial

    private var updatingFromDevice = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        FirebaseApp.initializeApp(this)
        setContentView(R.layout.activity_main)

        // Gắn các view từ layout
        tvTemp = findViewById(R.id.tvTemp)
        tvHumid = findViewById(R.id.tvHumid)
        tvStatus = findViewById(R.id.tvStatus)
        swRL1 = findViewById(R.id.swRL1)
        swRL2 = findViewById(R.id.swRL2)
        swRL3 = findViewById(R.id.swRL3)

        auth = FirebaseAuth.getInstance()
        signInAndBind()
    }

    // Đăng nhập Firebase bằng tài khoản dùng chung với ESP32
    private fun signInAndBind() {
        tvStatus.text = "Signing in..."
        val email = "42200368@student.tdtu.edu.vn"
        val password = "Nvt@3112004"

        auth.signInWithEmailAndPassword(email, password)
            .addOnSuccessListener {
                tvStatus.text = "Signed in"
                attachRealtime()
                attachRelayWriters()
            }
            .addOnFailureListener { e ->
                tvStatus.text = "Sign in failed: ${e.message}"
            }
    }

    // Lắng nghe dữ liệu DHT22 và trạng thái relay từ Firebase
    private fun attachRealtime() {
        val dhtTemp = db.getReference("ESP32/DHT22/Temp")
        val dhtHumid = db.getReference("ESP32/DHT22/Humid")

        dhtTemp.addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val temp = snapshot.getValue(Double::class.java)
                if (temp != null) tvTemp.text = String.format("Temp: %.1f °C", temp)
            }
            override fun onCancelled(error: DatabaseError) {}
        })

        dhtHumid.addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val humid = snapshot.getValue(Double::class.java)
                if (humid != null) tvHumid.text = String.format("Humid: %.1f %%", humid)
            }
            override fun onCancelled(error: DatabaseError) {}
        })

        val relayStateRef = db.getReference("ESP32/RelayState")
        relayStateRef.keepSynced(true)

        fun updateSwitch(sw: SwitchMaterial, on: Boolean) {
            updatingFromDevice = true
            sw.isChecked = on
            updatingFromDevice = false
        }

        relayStateRef.child("RL1").addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val state = snapshot.getValue(String::class.java)?.uppercase()
                if (state != null) updateSwitch(swRL1, state == "ON")
            }
            override fun onCancelled(error: DatabaseError) {}
        })

        relayStateRef.child("RL2").addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val state = snapshot.getValue(String::class.java)?.uppercase()
                if (state != null) updateSwitch(swRL2, state == "ON")
            }
            override fun onCancelled(error: DatabaseError) {}
        })

        relayStateRef.child("RL3").addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val state = snapshot.getValue(String::class.java)?.uppercase()
                if (state != null) updateSwitch(swRL3, state == "ON")
            }
            override fun onCancelled(error: DatabaseError) {}
        })
    }

    // Khi người dùng bật/tắt công tắc → gửi lên Firebase
    private fun attachRelayWriters() {
        val relayRef = db.getReference("ESP32/Relay")

        fun bindSwitch(sw: SwitchMaterial, key: String) {
            sw.setOnCheckedChangeListener { _, isChecked ->
                if (updatingFromDevice) return@setOnCheckedChangeListener
                relayRef.child(key).setValue(if (isChecked) "ON" else "OFF")
            }
        }

        bindSwitch(swRL1, "RL1")
        bindSwitch(swRL2, "RL2")
        bindSwitch(swRL3, "RL3")
    }
}
