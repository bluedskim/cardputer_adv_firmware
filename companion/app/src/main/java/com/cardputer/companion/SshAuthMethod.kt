package com.cardputer.companion

enum class SshAuthMethod {
    PASSWORD,
    PRIVATE_KEY;

    companion object {
        fun fromStored(value: String?): SshAuthMethod = when (value?.lowercase()) {
            "private_key", "key" -> PRIVATE_KEY
            else -> PASSWORD
        }

        fun toStored(method: SshAuthMethod): String = when (method) {
            PASSWORD -> "password"
            PRIVATE_KEY -> "private_key"
        }
    }
}
