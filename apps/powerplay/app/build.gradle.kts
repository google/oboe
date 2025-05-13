import com.android.build.api.dsl.Packaging

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.plugin.compose)
}

android {
    namespace = "com.android.oboe.powerplay"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.android.oboe.powerplay"
        minSdk = 32
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        vectorDrawables {
            useSupportLibrary = true
        }

        ndk {
            // Requires NDK r29 for PCM Offload
            abiFilters.addAll(listOf("armeabi-v7a", "x86", "x86_64", "arm64-v8a"))
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_18
        targetCompatibility = JavaVersion.VERSION_18
    }

    kotlinOptions {
        jvmTarget = "18"
    }

    buildFeatures {
        compose = true
    }

    ndkVersion = "29.0.13113456 rc1"

    externalNativeBuild {
        cmake {
            // define the path where the CMakeList has been put for this module.
            path("CMakeLists.txt")

            // specify the CMake version we want to use.
            version = "3.31.6"
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

dependencies {
    // Core components
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.activity.ktx)

    // Lifecycle components
    implementation(libs.androidx.lifecycle.process)
    implementation(libs.androidx.lifecycle.viewmodel)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.viewmodel.ktx)
    implementation(libs.androidx.lifecycle.runtime.compose)

    // Compose components
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.activity.compose)
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.coroutines.android)

    // Testing components
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    debugImplementation(libs.androidx.ui.tooling)
}