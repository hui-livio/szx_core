import { expect, it } from 'vitest'
import { readFileSync } from 'node:fs'
import zh from '../src/i18n/locales/zh-CN'
import en from '../src/i18n/locales/en-US'
import ja from '../src/i18n/locales/ja-JP'
import ko from '../src/i18n/locales/ko-KR'
import fr from '../src/i18n/locales/fr-FR'
import ru from '../src/i18n/locales/ru-RU'

it.each([
  ['zh-CN', zh],
  ['en-US', en],
  ['ja-JP', ja],
  ['ko-KR', ko],
  ['fr-FR', fr],
  ['ru-RU', ru],
] as const)(
  'provides explicit RBAC/appearance translations with matching placeholders in %s',
  (locale, messages) => {
    const source = readFileSync(`src/i18n/locales/${locale}.ts`, 'utf8')
    for (const section of ['roleManagement', 'rolePermissions', 'rbac', 'appearance'] as const) {
      expect(source).toContain(`${section}: {`)
      expect(Object.keys(messages[section]).sort()).toEqual(Object.keys(zh[section]).sort())
      for (const key of Object.keys(zh[section])) {
        const translated = (messages[section] as Record<string, string>)[key]!
        const original = (zh[section] as Record<string, string>)[key]!
        expect(translated.trim()).not.toBe('')
        expect([...translated.matchAll(/\{\w+\}/g)].map((match) => match[0]).sort()).toEqual(
          [...original.matchAll(/\{\w+\}/g)].map((match) => match[0]).sort(),
        )
      }
    }
    expect(source).toContain('rbac_permission_changed:')
    expect(source).toContain('rbac_role_changed:')
  },
)
