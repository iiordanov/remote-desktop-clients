package com.morpheusly.common

import android.app.Activity
import android.content.ContentResolver
import android.content.Intent
import android.net.Uri
import java.io.*

class Utilities {
    companion object {
        private const val BUFFER_SIZE = 3000

        /**
         * Outputs the given InputStream to a file.
         * @param toOutput
         * @param out
         * @throws IOException
         */
        fun outputToStream(toOutput: String, out: OutputStream) {
            val bos = BufferedOutputStream(out)
            try {
                bos.write(toOutput.toByteArray())
                bos.close()
                out.close()
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }

        fun readFileToUtf8String(i: InputStream?): String? {
            val bis = BufferedInputStream(i)
            val buffer = ByteArrayOutputStream()
            try {
                val data = ByteArray(BUFFER_SIZE)
                var current: Int
                while (bis.read(data, 0, data.size).also { current = it } != -1) {
                    buffer.write(data, 0, current)
                }
                i?.close()
                return buffer.toString("UTF-8")
            } catch (e: IOException) {
                e.printStackTrace()
            }
            return ""
        }

        fun getInputStreamFromUri(resolver: ContentResolver, uri: Uri?): InputStream? {
            var stream: InputStream? = null
            try {
                stream = resolver.openInputStream(uri!!)
            } catch (e: IOException) {
                e.printStackTrace()
            }
            return stream
        }

        fun getOutputStreamFromUri(resolver: ContentResolver, uri: Uri?): OutputStream? {
            var out: OutputStream? = null
            try {
                out = resolver.openOutputStream(uri!!, "wt")
            } catch (e: IOException) {
                e.printStackTrace()
            }
            return out
        }

        fun getStringDataFromIntent(data: Intent, activity: Activity): String? {
            val resolver = activity.contentResolver
            val stream: InputStream? = getInputStreamFromUri(resolver, data.data)
            return readFileToUtf8String(stream)
        }
    }
}