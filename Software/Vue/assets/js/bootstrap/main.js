import { computedOptions } from '../app/computed-options.js?v=9';
import { lifecycleOptions } from '../app/lifecycle-hooks.js?v=9';
import { methodsOptions } from '../app/methods/index.js?v=9';
import { createInitialState } from '../app/state-factory.js?v=9';

const { createApp } = Vue;

// 检测是否为微信浏览器
function isWechatBrowser() {
    const ua = navigator.userAgent.toLowerCase();
    return ua.indexOf('micromessenger') !== -1;
}

// 全局存储微信浏览器检测结果
window.isWechat = isWechatBrowser();

// 如果是微信浏览器，添加样式类
if (window.isWechat) {
    document.documentElement.classList.add('wechat-browser');
}

function syncAppViewportHeight() {
    const height = window.innerHeight;
    document.documentElement.style.setProperty('--app-height', `${height}px`);
}

syncAppViewportHeight();
window.addEventListener('resize', syncAppViewportHeight, { passive: true });
window.addEventListener('orientationchange', syncAppViewportHeight, { passive: true });
window.addEventListener('pageshow', syncAppViewportHeight, { passive: true });

createApp({
    data: createInitialState,
    computed: computedOptions,
    methods: methodsOptions,
    ...lifecycleOptions
}).mount('#app');
