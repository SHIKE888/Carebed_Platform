import { computedOptions } from '../app/computed-options.js';
import { lifecycleOptions } from '../app/lifecycle-hooks.js';
import { methodsOptions } from '../app/methods/index.js';
import { createInitialState } from '../app/state-factory.js';

const { createApp } = Vue;

createApp({
    data: createInitialState,
    computed: computedOptions,
    methods: methodsOptions,
    ...lifecycleOptions
}).mount('#app');
