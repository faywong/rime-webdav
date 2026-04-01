/*#***************************************************************************
#                         __    __   _____       _____
#   Project              |  |  |  | |     \     /  ___|
#                        |  |__|  | |  |\  \   /  /
#                        |        | |  | )  ) (  (     
#                        |   /\   | |  |/  /   \  \___
#                         \_/  \_/  |_____/     \_____|
#
# Copyright (C) 2016, The WDC Project, <designerror@yandex.ru>, et al.
#
# This software is licensed as described in the file LICENSE, which
# you should have received as part of this distribution. 
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the LICENSE file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
############################################################################*/

#include "request.hpp"
#include "fsinfo.hpp"

#include <sys/stat.h>
#include <chrono>
#include <thread>

#if defined(__ANDROID__) && defined(__has_include)
#if __has_include(<android/log.h>)
#include <android/log.h>
#define WEBDAV_ANDROID_LOG 1
#endif
#endif

namespace WebDAV
{
	static bool dir_exists(const char* path) {
		if (path == nullptr || *path == '\0') return false;
		struct stat st;
		if (stat(path, &st) != 0) return false;
		return S_ISDIR(st.st_mode);
	}

	auto inline get(const dict_t& options, const std::string&& name) -> std::string
	{
		auto it = options.find(name);
		if (it == options.end()) {
            return std::string{""};
        }
		else {
            return it->second;
        }
	}

	Request::Request(dict_t&& options_) : options(options_)
	{
		auto webdav_hostname = get(options, "webdav_hostname");
		auto webdav_username = get(options, "webdav_username");
		auto webdav_password = get(options, "webdav_password");

		auto proxy_hostname = get(options, "proxy_hostname");
		auto proxy_username = get(options, "proxy_username");
		auto proxy_password = get(options, "proxy_password");

		auto cert_path = get(options, "cert_path");
		auto key_path = get(options, "key_path");

		this->handle = curl_easy_init();

		this->set(CURLOPT_NOSIGNAL, 1L);
		this->set(CURLOPT_FOLLOWLOCATION, 1L);
		this->set(CURLOPT_MAXREDIRS, 5L);
		this->set(CURLOPT_CONNECTTIMEOUT, 15L);
		this->set(CURLOPT_TIMEOUT, 60L);

		const char* capath = nullptr;
		if (dir_exists("/apex/com.android.conscrypt/cacerts")) {
			capath = "/apex/com.android.conscrypt/cacerts";
		} else if (dir_exists("/system/etc/security/cacerts")) {
			capath = "/system/etc/security/cacerts";
		}
		if (capath != nullptr) {
			this->set(CURLOPT_CAPATH, capath);
			this->set(CURLOPT_SSL_VERIFYHOST, 2L);
			this->set(CURLOPT_SSL_VERIFYPEER, 1L);
		} else {
			this->set(CURLOPT_SSL_VERIFYHOST, 0L);
			this->set(CURLOPT_SSL_VERIFYPEER, 0L);
		}

#ifdef _DEBUG
		this->set(CURLOPT_VERBOSE, 1);
#else
		this->set(CURLOPT_VERBOSE, 0);
#endif
		if (this->cert_required()){

			this->set(CURLOPT_SSLCERTTYPE, "PEM");
			this->set(CURLOPT_SSLKEYTYPE, "PEM");
			this->set(CURLOPT_SSLCERT, const_cast<char *>(cert_path.c_str()));
			this->set(CURLOPT_SSLKEY, const_cast<char *>(key_path.c_str()));

		}
		
		this->set(CURLOPT_URL, const_cast<char *>(webdav_hostname.c_str()));
		this->set(CURLOPT_HTTPAUTH, static_cast<int>(CURLAUTH_BASIC));
		auto token = webdav_username + ":" + webdav_password;
		this->set(CURLOPT_USERPWD, const_cast<char *>(token.c_str()));

		if (!this->proxy_enabled()) return;

		this->set(CURLOPT_PROXY, const_cast<char *>(proxy_hostname.c_str()));
		this->set(CURLOPT_PROXYAUTH, static_cast<int>(CURLAUTH_BASIC));

		if (proxy_username.empty()) return;

		if (proxy_password.empty())
		{
			this->set(CURLOPT_PROXYUSERNAME, const_cast<char *>(proxy_username.c_str()));
		}
		else
		{
			token = proxy_username + ":" + proxy_password;
			this->set(CURLOPT_PROXYUSERPWD, const_cast<char *>(token.c_str()));
		}
	}

	Request::~Request() noexcept
	{
		if (this->handle != nullptr) curl_easy_cleanup(this->handle);
	}


    auto Request::swap(Request& other) noexcept -> void
    {
        using std::swap;
        swap(handle, other.handle);
    }

    Request::Request(Request&& other) noexcept : handle{ other.handle }
    {
        other.handle = nullptr;
    }

    auto Request::operator=(Request&& other) noexcept -> Request &
    {
        if (this != &other) {
            Request(std::move(other)).swap(*this);
        }

        return *this;
    }

	bool Request::perform() const noexcept
	{
		if (this->handle == nullptr) return false;
		constexpr int kMaxAttempts = 3;
		for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
			CURLcode res = curl_easy_perform(this->handle);
			if (res != CURLE_OK) {
				bool retryable = (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT ||
				                 res == CURLE_SEND_ERROR || res == CURLE_RECV_ERROR ||
				                 res == CURLE_GOT_NOTHING);
#if defined(WEBDAV_ANDROID_LOG)
			const char* effective_url = nullptr;
			curl_easy_getinfo(this->handle, CURLINFO_EFFECTIVE_URL, &effective_url);
			__android_log_print(ANDROID_LOG_ERROR, "WebDAV",
							"webdav curl failed: code=%d msg=%s url=%s",
							static_cast<int>(res), curl_easy_strerror(res), effective_url ? effective_url : "");
#endif
				if (!retryable || attempt + 1 >= kMaxAttempts) return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(200 * (1 << attempt)));
				continue;
			}

			long http_code = 0;
			curl_easy_getinfo(this->handle, CURLINFO_RESPONSE_CODE, &http_code);
			if ((http_code < 200 || http_code > 299) && http_code != 207) {
				bool retryable = (http_code == 429 || http_code == 500 || http_code == 502 || http_code == 503 ||
				                 http_code == 504);
#if defined(WEBDAV_ANDROID_LOG)
			const char* effective_url = nullptr;
			const char* effective_method = nullptr;
			curl_easy_getinfo(this->handle, CURLINFO_EFFECTIVE_URL, &effective_url);
			curl_easy_getinfo(this->handle, CURLINFO_EFFECTIVE_METHOD, &effective_method);
			if (http_code == 404 && effective_method != nullptr && std::string(effective_method) == "PROPFIND") {
				return false;
			}
			__android_log_print(ANDROID_LOG_ERROR, "WebDAV",
							"webdav http failed: status=%ld url=%s",
							http_code, effective_url ? effective_url : "");
#endif
				if (!retryable || attempt + 1 >= kMaxAttempts) return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(200 * (1 << attempt)));
				continue;
			}

			return true;
		}
		return false;
	}

	bool Request::proxy_enabled() const noexcept
	{
		auto proxy_hostname = get(options, "proxy_hostname");
		auto proxy_username = get(options, "proxy_username");
		auto proxy_password = get(options, "proxy_password");
		bool proxy_hostname_presented = !proxy_hostname.empty();
		if (!proxy_hostname_presented) return false;
		bool proxy_username_presented = !proxy_username.empty();
		bool proxy_password_presented = !proxy_password.empty();
		if (proxy_password_presented && !proxy_username_presented) return false;
		return true;
	}

	bool Request::cert_required() const noexcept
	{
		const auto cert_path = get(options, "cert_path");
		const auto key_path = get(options, "key_path");
		if (cert_path.empty()) return false;
		bool cert_is_existed = FileInfo::exists(cert_path);
		if (!cert_is_existed) return false;
		if (key_path.empty()) return false;
		return FileInfo::exists(key_path);
	}
} // namespace WebDAV
