plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.cardputer.companion"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.cardputer.companion"
        minSdk = 31
        targetSdk = 35
        versionCode = 11
        versionName = "0.8.9"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // CI / sideload: no store keystore yet — sign with the debug key.
            signingConfig = signingConfigs.getByName("debug")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        viewBinding = true
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.0")
    implementation("androidx.activity:activity-ktx:1.9.3")
    implementation("androidx.security:security-crypto:1.1.0-alpha06")
    implementation("com.hierynomus:sshj:0.38.0")
    // Explicit BC so Android's incomplete system "BC" provider can be replaced at runtime.
    implementation("org.bouncycastle:bcprov-jdk18on:1.75")
    implementation("org.bouncycastle:bcpkix-jdk18on:1.75")
}
