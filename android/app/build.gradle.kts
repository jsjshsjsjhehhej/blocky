plugins {
    id("com.android.application")
}

android {
    namespace = "com.blocky.preview"
    compileSdk = 37
    ndkVersion = "27.0.12077973"

    defaultConfig {
        applicationId = "com.blocky.preview"
        minSdk = 26 // Android 8.0: Blocky non gira su Android 7
        targetSdk = 37
        versionCode = 1
        versionName = "0.1-fase1"
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    // La cartella assets/ del repo entra nell APK cosi' com e (stessi file
    // che su Windows stanno accanto all eseguibile). Non viene duplicata nel
    // progetto Android: si punta direttamente a quella vera.
    sourceSets {
        getByName("main") {
            assets.srcDirs("../../assets")
        }
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation("androidx.games:games-activity:4.0.0")
    implementation("androidx.appcompat:appcompat:1.8.0")
    implementation("androidx.core:core-ktx:1.19.0")
    implementation("com.google.android.material:material:1.14.0")
}
