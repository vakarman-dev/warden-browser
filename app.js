const translations = {
  ru: {
    nav_features: "Возможности",
    nav_download: "Скачать",
    hero_title: "Приватность на уровне ядра. Скорость C++20.",
    hero_subtitle: "Warden Browser — современный кроссплатформенный веб-браузер на базе Qt6 WebEngine со сквозным шифрованием аккаунтов, встроенным Tor-прокси и боковой панелью бесплатных нейросетей.",
    scroll_text: "Прокрутите ниже, чтобы скачать",
    features_title: "Ключевые возможности",
    feat1_title: "Защита на уровне C++ ядра",
    feat1_desc: "Нативный перехватчик трафика блокирует трекеры и рекламу до загрузки, принудительно включает HTTPS и защищает от утечек WebRTC.",
    feat2_title: "Tor / SOCKS5 и «Сжечь сессию»",
    feat2_desc: "Маршрутизация трафика через Tor в один клик и мгновенное уничтожение куки, кэша и всех открытых вкладок одной кнопкой.",
    feat3_title: "Шифрование аккаунтов E2EE",
    feat3_desc: "Изолированные профили пользователей с клиентским шифрованием AES-256-GCM перед отправкой в облако.",
    feat4_title: "Боковая панель нейросетей",
    feat4_desc: "Встроенный доступ к бесплатным ИИ (ChatGPT, DuckDuckGo AI, HuggingChat) с автоматическим переключением при исчерпании лимита.",
    feat5_title: "Кастомная новая вкладка",
    feat5_desc: "Выбор поисковой системы (DuckDuckGo, Google, Яндекс) и настройка фона: живые обои, свои изображения или сплошной цвет.",
    feat6_title: "100% Open Source",
    feat6_desc: "Открытый исходный код на современном C++20 и Qt6 для Windows, Linux и macOS. Никакой скрытой телеметрии.",
    dl_box_title: "Готовы попробовать Warden Browser?",
    dl_box_desc: "Узнайте о статусе первой сборки для Windows и Linux или протестируйте интерактивный прототип прямо сейчас.",
    btn_download: "Скачать Warden Browser",
    status_badge: "🚧 В активной разработке",
    status_title: "Первая версия (v0.1) выйдет уже скоро!",
    status_desc: "Сейчас мы активно собираем нативную версию Warden Browser на C++20 и Qt6 WebEngine для Windows и Linux. Пока идет подготовка релиза, вы уже можете перейти на наш GitHub и посмотреть интерактивный прототип (v0.0-prototype-test)!",
    btn_goto_github: "Перейти на GitHub и посмотреть прототип",
    btn_back: "← Вернуться на главную",
    footer_text: "Создано с душой для приватного интернета • Автор: Alexey (vakarman-dev) • "
  },
  en: {
    nav_features: "Features",
    nav_download: "Download",
    hero_title: "Core-Level Privacy. C++20 Speed.",
    hero_subtitle: "Warden Browser is a modern cross-platform desktop web browser powered by Qt6 WebEngine with E2EE encrypted accounts, built-in Tor proxy, and an auto-switching AI sidebar.",
    scroll_text: "Scroll down to download",
    features_title: "Key Features",
    feat1_title: "Core-Level C++ Protection",
    feat1_desc: "Native request interceptor blocks trackers and ads before loading, enforces HTTPS-Only mode, and prevents WebRTC IP leaks.",
    feat2_title: "Tor / SOCKS5 & Burn Session",
    feat2_desc: "One-click routing through a local Tor proxy and instant destruction of cookies, cache, and active tabs with a single button.",
    feat3_title: "E2EE Account Encryption",
    feat3_desc: "Isolated local SQLite user profiles with client-side AES-256-GCM encryption before cloud synchronization.",
    feat4_title: "Auto-Switching AI Sidebar",
    feat4_desc: "Built-in side panel with free AI assistants (ChatGPT, DuckDuckGo AI, HuggingChat) that automatically switches when limits are reached.",
    feat5_title: "Customizable New Tab",
    feat5_desc: "Choose your default search engine (DuckDuckGo, Google, Yandex) and customize backgrounds with live wallpapers, local images, or solid colors.",
    feat6_title: "100% Open Source",
    feat6_desc: "Clean, transparent C++20 and Qt6 source code for Windows, Linux, and macOS. Zero hidden telemetry.",
    dl_box_title: "Ready to try Warden Browser?",
    dl_box_desc: "Check the status of the upcoming v0.1 release for Windows and Linux or test our interactive web prototype right now.",
    btn_download: "Download Warden Browser",
    status_badge: "🚧 In Active Development",
    status_title: "The first version (v0.1) is coming very soon!",
    status_desc: "We are currently building the native C++20 & Qt6 WebEngine release of Warden Browser for Windows and Linux. While it's in the works, you can visit our GitHub repository to check out the interactive prototype (v0.0-prototype-test)!",
    btn_goto_github: "Go to GitHub & View Prototype",
    btn_back: "← Back to Home",
    footer_text: "Built with passion for a private internet • Created by Alexey (vakarman-dev) • "
  }
};

const flagSVGs = {
  ru: `<svg class="flag-icon" viewBox="0 0 3 2"><rect width="3" height="2" fill="#fff"/><rect width="3" height="1.333" y="0.667" fill="#0039a6"/><rect width="3" height="0.667" y="1.333" fill="#d52b1e"/></svg>`,
  en: `<svg class="flag-icon" viewBox="0 0 60 30"><rect width="60" height="30" fill="#012169"/><path d="M0,0 L60,30 M60,0 L0,30" stroke="#fff" stroke-width="6"/><path d="M30,0 v30 M0,15 h60" stroke="#fff" stroke-width="10"/><path d="M30,0 v30 M0,15 h60" stroke="#C8102E" stroke-width="6"/></svg>`
};

const langDropdown = document.getElementById("langDropdown");
const langBtn = document.getElementById("langBtn");

langBtn.addEventListener("click", (e) => {
  e.stopPropagation();
  langDropdown.classList.toggle("open");
});

document.addEventListener("click", () => {
  langDropdown.classList.remove("open");
});

function setLanguage(lang) {
  if (!translations[lang]) return;
  localStorage.setItem("warden_site_lang", lang);
  document.documentElement.lang = lang;

  document.getElementById("currentFlag").innerHTML = flagSVGs[lang];
  document.getElementById("currentLangCode").textContent = lang.toUpperCase();

  document.querySelectorAll(".lang-option").forEach(btn => {
    btn.classList.toggle("active", btn.dataset.lang === lang);
  });

  document.querySelectorAll("[data-i18n]").forEach(el => {
    const key = el.getAttribute("data-i18n");
    if (translations[lang][key]) {
      el.textContent = translations[lang][key];
    }
  });

  langDropdown.classList.remove("open");
}

setLanguage(localStorage.getItem("warden_site_lang") || "ru");

const mainView = document.getElementById("mainView");
const statusView = document.getElementById("statusView");

function showReleaseStatusView() {
  mainView.classList.add("hidden-view");
  statusView.classList.remove("hidden-view");
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function showMainView() {
  statusView.classList.add("hidden-view");
  mainView.classList.remove("hidden-view");
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function navigateToSection(sectionId) {
  if (mainView.classList.contains("hidden-view")) {
    showMainView();
    setTimeout(() => {
      const el = document.getElementById(sectionId);
      if (el) el.scrollIntoView({ behavior: "smooth" });
    }, 100);
  } else {
    const el = document.getElementById(sectionId);
    if (el) el.scrollIntoView({ behavior: "smooth" });
  }
}

const canvas = document.getElementById("bg-canvas");
const ctx = canvas.getContext("2d");
let width, height, particles;
const mouse = { x: null, y: null, radius: 130 };

window.addEventListener("mousemove", (e) => {
  mouse.x = e.clientX;
  mouse.y = e.clientY;
});

window.addEventListener("mouseout", () => {
  mouse.x = null;
  mouse.y = null;
});

function initCanvas() {
  width = canvas.width = window.innerWidth;
  height = canvas.height = window.innerHeight;
  const count = Math.min(Math.floor((width * height) / 16000), 80);
  particles = [];
  for (let i = 0; i < count; i++) {
    particles.push({
      x: Math.random() * width,
      y: Math.random() * height,
      vx: (Math.random() - 0.5) * 0.6,
      vy: (Math.random() - 0.5) * 0.6,
      size: Math.random() * 2 + 1
    });
  }
}

function animateCanvas() {
  ctx.clearRect(0, 0, width, height);
  for (let i = 0; i < particles.length; i++) {
    const p = particles[i];
    p.x += p.vx;
    p.y += p.vy;

    if (p.x < 0 || p.x > width) p.vx *= -1;
    if (p.y < 0 || p.y > height) p.vy *= -1;

    ctx.beginPath();
    ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
    ctx.fillStyle = "rgba(74, 144, 226, 0.55)";
    ctx.fill();

    for (let j = i + 1; j < particles.length; j++) {
      const p2 = particles[j];
      const dx = p.x - p2.x;
      const dy = p.y - p2.y;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < 120) {
        ctx.beginPath();
        ctx.strokeStyle = `rgba(74, 144, 226, ${0.18 * (1 - dist / 120)})`;
        ctx.lineWidth = 1;
        ctx.moveTo(p.x, p.y);
        ctx.lineTo(p2.x, p2.y);
        ctx.stroke();
      }
    }

    if (mouse.x !== null && mouse.y !== null) {
      const dx = p.x - mouse.x;
      const dy = p.y - mouse.y;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < mouse.radius) {
        ctx.beginPath();
        ctx.strokeStyle = `rgba(0, 210, 255, ${0.35 * (1 - dist / mouse.radius)})`;
        ctx.lineWidth = 1.2;
        ctx.moveTo(p.x, p.y);
        ctx.lineTo(mouse.x, mouse.y);
        ctx.stroke();
      }
    }
  }
  requestAnimationFrame(animateCanvas);
}

window.addEventListener("resize", initCanvas);
initCanvas();
animateCanvas();
