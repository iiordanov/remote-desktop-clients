package com.undatech.opaque.util

import java.io.File

object FileUtilsKt {
    fun readFileDirectlyAsText(fileName: String): String
            = File(fileName).readText(Charsets.UTF_8)
}