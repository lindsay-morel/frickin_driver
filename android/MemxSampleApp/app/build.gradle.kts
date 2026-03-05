plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.memryx.memxdriver"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.memryx.memxdriver"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        setProperty("android.nonSdkApiUsage", "warning")
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        viewBinding = true
        aidl = true
    }
    ndkVersion = "26.1.10909125"
}

dependencies {

    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)
    implementation(libs.navigation.fragment)
    implementation(libs.navigation.ui)
    implementation("androidx.annotation:annotation:1.9.1")
    implementation(project(":app:nativelib"))
    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
    androidTestImplementation(libs.espresso.core)
}