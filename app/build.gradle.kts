plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("com.google.gms.google-services")
}

android {
    namespace = "com.example.padora"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.padora"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isMinifyEnabled = false
        }
    }

    // Tắt Compose (vì bạn đang dùng layout XML)
    buildFeatures {
        compose = false
        viewBinding = false
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    // Firebase BOM: để đồng bộ version các thư viện Firebase
    implementation(platform("com.google.firebase:firebase-bom:33.6.0"))

    // Firebase Auth + Realtime Database
    implementation("com.google.firebase:firebase-auth-ktx")
    implementation("com.google.firebase:firebase-database-ktx")

    // Material Design (SwitchMaterial)
    implementation("com.google.android.material:material:1.12.0")

    // Các thư viện Android cơ bản
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")

    // (Không thêm Compose libraries nữa)
}
