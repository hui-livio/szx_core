import { createI18n } from 'vue-i18n'

import enUS from './locales/en-US'
import enGB from './locales/en-GB'
import frFR from './locales/fr-FR'
import jaJP from './locales/ja-JP'
import koKR from './locales/ko-KR'
import ruRU from './locales/ru-RU'
import zhCN from './locales/zh-CN'

export const localeOptions = [
  { value: 'zh-CN', label: '简体中文' },
  { value: 'ja-JP', label: '日本語' },
  { value: 'ko-KR', label: '한국어' },
  { value: 'en-GB', label: 'English (UK)' },
  { value: 'en-US', label: 'English (US)' },
  { value: 'fr-FR', label: 'Français' },
  { value: 'ru-RU', label: 'Русский' },
] as const

export type SupportedLocale = (typeof localeOptions)[number]['value']

const supportedLocales = new Set<SupportedLocale>(localeOptions.map(({ value }) => value))
const browserLocale = typeof navigator === 'undefined' ? '' : navigator.language
const savedLocale =
  typeof localStorage === 'undefined' ? '' : localStorage.getItem('szx-admin-locale')

function resolveLocale(value: string | null | undefined): SupportedLocale {
  if (value && supportedLocales.has(value as SupportedLocale)) return value as SupportedLocale
  const match = localeOptions.find(
    ({ value: option }) => option.split('-')[0] === value?.split('-')[0],
  )
  return match?.value ?? 'zh-CN'
}

export const i18n = createI18n({
  legacy: false,
  locale: resolveLocale(savedLocale || browserLocale),
  fallbackLocale: 'zh-CN',
  messages: {
    'zh-CN': zhCN,
    'ja-JP': jaJP,
    'ko-KR': koKR,
    'en-US': enUS,
    'en-GB': enGB,
    'fr-FR': frFR,
    'ru-RU': ruRU,
  },
})

export function setLocale(locale: SupportedLocale) {
  i18n.global.locale.value = locale
  if (typeof localStorage !== 'undefined') localStorage.setItem('szx-admin-locale', locale)
  if (typeof document !== 'undefined') document.documentElement.lang = locale
}

export function translate(key: string, values?: Record<string, unknown>): string {
  return i18n.global.t(key, values ?? {})
}

if (typeof document !== 'undefined') document.documentElement.lang = i18n.global.locale.value
