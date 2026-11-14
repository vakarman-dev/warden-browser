<div align="center">

<img src="resources/warden-browser-logo.png" alt="Warden Browser Logo" width="400" height="400" />

# Warden Browser

**Privacy-Hardened, Cross-Platform Desktop Web Browser built on C++20 & Qt6 WebEngine**

[![Website](https://shields.io)](https://github.io)
![Commit Activity](https://shields.io)
![Repo Size](https://shields.io)
![Stars](https://shields.io)
![Contributors](https://shields.io)
![License](https://shields.io)
![Latest Release](https://shields.io)

</div>

---

## 🛡️ About Warden Browser

**Warden Browser** is an open-source, privacy-first desktop web browser built with **C++20** and **Qt6 WebEngine**. It features native C++ tracker blocking, Tor routing support, zero-knowledge encrypted local profiles, an AI sidebar, and a clean interface without hidden telemetry.

### ✨ Key Features

* **Core-Level C++ Protection (`PrivacyEngine`):** Blocks ads and trackers natively, enforces HTTPS-Only mode, strips cross-site `Referer` headers, and prevents WebRTC IP leaks.
* **Tor / SOCKS5 & Burn Session:** Route traffic through Tor or use the **Burn Session** button to instantly clear cookies, cache, history, and tabs.
* **E2EE Encrypted Accounts:** Client-side **AES-256-GCM** encryption (`PBKDF2`) for local SQLite profiles before optional cloud sync.
* **Auto-Switching AI Sidebar:** Integrated side panel with free AI assistants (ChatGPT, DuckDuckGo AI, HuggingChat) and automatic fallback.
* **Customizable New Tab:** Supports custom search engines (DuckDuckGo, Google, Yandex) and interactive particle or wallpaper backgrounds.
* **100% Open Source:** Modern C++20 and Qt6 codebase for Windows, Linux, and macOS.

---

## 🚀 Status & Roadmap

* **Stage 1 (`v0.0-prototype-test`):** Interactive prototype, website, and cloud database setup.
*  **Stage 2 (`v0.1-test` — In Progress):** Native C++20 & Qt6 WebEngine builds via GitHub Actions.
* **Stage 3 (`v1.0`):** First stable release with full packaging and extended customization.

---

## 📄 License

Licensed under the terms of the [LICENSE](LICENSE) file in this repository.
