// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("de.undercouch.download") version "5.5.0"
}

import android.databinding.tool.ext.capitalizeUS
import de.undercouch.gradle.tasks.download.Download

/**
 * Use the number of seconds/10 since Jan 1 2016 as the versionCode.
 * This lets us upload a new build at most every 10 seconds for the
 * next 680 years.
 */
val autoVersion = (((System.currentTimeMillis() / 1000) - 1451606400) / 10).toInt()
val abiFilter = listOf("arm64-v8a", "x86_64")

val downloadedJniLibsPath = "${buildDir}/downloadedJniLibs"

@Suppress("UnstableApiUsage")
android {
    namespace = "org.citra.citra_emu"

    compileSdkVersion = "android-33"
    ndkVersion = "25.2.9519653"

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

    lint {
        // This is important as it will run lint but not abort on error
        // Lint has some overly obnoxious "errors" that should really be warnings
        abortOnError = false
    }

    defaultConfig {
        // TODO If this is ever modified, change application_id in strings.xml
        applicationId = "org.citra.citra_emu"
        minSdk = 28
        targetSdk = 33
        versionCode = autoVersion
        versionName = getGitVersion()

        ndk {
            //noinspection ChromeOsAbiSupport
            abiFilters += abiFilter
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0", // Don't use QT
                    "-DENABLE_SDL2=0", // Don't use SDL
                    "-DANDROID_ARM_NEON=true" // cryptopp requires Neon to work
                )
            }
        }
    }

    val keystoreFile = System.getenv("ANDROID_KEYSTORE_FILE")
    if (keystoreFile != null) {
        signingConfigs {
            create("release") {
                storeFile = file(keystoreFile)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASS")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEYSTORE_PASS")
            }
        }
    }

    // Define build types, which are orthogonal to product flavors.
    buildTypes {
        // Signed by release key, allowing for upload to Play Store.
        release {
            signingConfig = if (keystoreFile != null) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }

        // builds a release build that doesn't need signing
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        register("relWithDebInfo") {
            initWith(getByName("release"))
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
            signingConfig = signingConfigs.getByName("debug")
            isMinifyEnabled = false
            isDebuggable = true
            isJniDebuggable = true
        }

        // Signed by debug key disallowing distribution on Play Store.
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        debug {
            // TODO If this is ever modified, change application_id in debug/strings.xml
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
            isDebuggable = true
            isJniDebuggable = true
        }
    }

    flavorDimensions.add("version")
    productFlavors {
        create("canary") {
            dimension = "version"
            applicationIdSuffix = ".canary"
        }

        create("nightly") {
            dimension = "version"
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("../../../CMakeLists.txt")
        }
    }

    sourceSets {
        named("main") {
            // Set up path for downloaded native libraries
            jniLibs.srcDir(downloadedJniLibsPath)
        }
    }
}

dependencies {
    implementation("androidx.activity:activity-ktx:1.7.2")
    implementation("androidx.fragment:fragment-ktx:1.6.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.6.1")
    implementation("androidx.slidingpanelayout:slidingpanelayout:1.2.0")
    implementation("com.google.android.material:material:1.9.0")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.work:work-runtime:2.8.1")

    // For loading huge screenshots from the disk.
    implementation("com.squareup.picasso:picasso:2.71828")

    // Allows FRP-style asynchronous operations in Android.
    implementation("io.reactivex:rxandroid:1.2.1")

    implementation("org.ini4j:ini4j:0.5.4")
    implementation("androidx.localbroadcastmanager:localbroadcastmanager:1.1.0")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")

    // Please don't upgrade the billing library as the newer version is not GPL-compatible
    implementation("com.android.billingclient:billing:2.0.3")
}

// Download Vulkan Validation Layers from the KhronosGroup GitHub.
val downloadVulkanValidationLayers = tasks.register<Download>("downloadVulkanValidationLayers") {
    src("https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/sdk-1.3.261.1/android-binaries-sdk-1.3.261.1-android.zip")
    dest(file("${buildDir}/tmp/Vulkan-ValidationLayers.zip"))
    onlyIfModified(true)
}

// Extract Vulkan Validation Layers into the downloaded native libraries directory.
val unzipVulkanValidationLayers = tasks.register<Copy>("unzipVulkanValidationLayers") {
    dependsOn(downloadVulkanValidationLayers)
    from(zipTree(downloadVulkanValidationLayers.get().dest)) {
        // Exclude the top level directory in the zip as it violates the expected jniLibs directory structure.
        eachFile {
            relativePath = RelativePath(true, *relativePath.segments.drop(1).toTypedArray())
        }
        includeEmptyDirs = false
    }
    into(downloadedJniLibsPath)
}

tasks.named("preBuild") {
    dependsOn(unzipVulkanValidationLayers)
}

fun getGitVersion(): String {
    var versionName = "0.0"

    try {
        versionName = ProcessBuilder("git", "describe", "--always", "--long")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
            .replace(Regex("(-0)?-[^-]+$"), "")
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy version number")
    }

    if (System.getenv("GITHUB_ACTIONS") != null) {
        val gitTag = System.getenv("GIT_TAG_NAME")
        versionName = gitTag ?: versionName
    }

    return versionName
}

android.applicationVariants.configureEach {
    val variant = this
    val capitalizedName = variant.name.capitalizeUS()

    val copyTask = tasks.register("copyBundle${capitalizedName}") {
        doLast {
            project.copy {
                from(variant.outputs.first().outputFile.parentFile)
                include("*.apk")
                into(layout.buildDirectory.dir("bundle"))
            }
            project.copy {
                from(layout.buildDirectory.dir("outputs/bundle/${variant.name}"))
                include("*.aab")
                into(layout.buildDirectory.dir("bundle"))
            }
        }
    }
    tasks.named("bundle${capitalizedName}").configure { finalizedBy(copyTask) }
}
