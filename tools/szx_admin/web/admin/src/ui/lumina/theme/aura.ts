import { definePreset } from '@primeuix/themes'
import Aura from '@primeuix/themes/aura'

// Use the existing CSS tokens so viewport scaling and Lumina remain the source of truth.
const surface = {
  0: 'var(--lg-n-0)',
  50: 'var(--lg-n-50)',
  100: 'var(--lg-n-100)',
  200: 'var(--lg-n-200)',
  300: 'var(--lg-n-300)',
  400: 'var(--lg-n-400)',
  500: 'var(--lg-n-500)',
  600: 'var(--lg-n-600)',
  700: 'var(--lg-n-700)',
  800: 'var(--lg-n-800)',
  900: 'var(--lg-n-900)',
  950: 'var(--lg-n-1000)',
}

const surfaces = {
  surface,
  formField: {
    background: 'var(--lg-field)',
    disabledBackground: 'var(--lg-glass-2)',
    borderColor: 'var(--lg-line)',
    hoverBorderColor: 'var(--lg-brand-300)',
    focusBorderColor: 'var(--lg-brand-400)',
    color: 'var(--text)',
    disabledColor: 'var(--muted)',
    placeholderColor: 'var(--muted)',
    iconColor: 'var(--muted)',
    shadow: 'none',
  },
  text: {
    color: 'var(--text)',
    hoverColor: 'var(--text)',
    mutedColor: 'var(--muted)',
    hoverMutedColor: 'var(--text)',
  },
  content: {
    background: 'var(--surface)',
    hoverBackground: 'var(--lg-field-focus)',
    borderColor: 'var(--lg-line)',
    color: 'var(--text)',
    hoverColor: 'var(--text)',
  },
  overlay: {
    select: {
      background: 'var(--surface)',
      borderColor: 'var(--lg-stroke)',
      color: 'var(--text)',
    },
    modal: {
      background: 'var(--surface)',
      borderColor: 'var(--lg-stroke)',
      color: 'var(--text)',
    },
  },
}

export const LuminaAura = definePreset(Aura, {
  primitive: {
    borderRadius: {
      xs: 'var(--lg-r-xs)',
      sm: 'var(--lg-r-sm)',
      md: 'var(--lg-r-xl)',
      lg: 'var(--lg-r-2xl)',
      xl: 'var(--lg-r-4xl)',
    },
  },
  semantic: {
    primary: {
      50: 'var(--lg-brand-50)',
      100: 'var(--lg-brand-100)',
      200: 'var(--lg-brand-200)',
      300: 'var(--lg-brand-300)',
      400: 'var(--lg-brand-400)',
      500: 'var(--lg-brand-500)',
      600: 'var(--lg-brand-600)',
      700: 'var(--lg-brand-700)',
      800: 'var(--lg-brand-800)',
      900: 'var(--lg-brand-900)',
      950: 'var(--lg-brand-900)',
    },
    focusRing: {
      width: '3px',
      color: 'var(--lg-brand-400)',
      offset: '3px',
    },
    formField: {
      paddingX: 'var(--lg-s5)',
      paddingY: 'var(--lg-s4)',
      borderRadius: 'var(--lg-r-xl)',
      focusRing: { shadow: 'var(--lg-ring-focus)' },
    },
    overlay: {
      select: { borderRadius: 'var(--lg-r-2xl)', shadow: 'var(--lg-sh-3)' },
      modal: {
        borderRadius: 'var(--lg-r-4xl)',
        padding: 'var(--lg-s12)',
        shadow: 'var(--lg-sh-card)',
      },
    },
    colorScheme: {
      light: surfaces,
      dark: surfaces,
    },
  },
  components: {
    button: {
      root: {
        borderRadius: 'var(--lg-r-xl)',
        paddingX: 'var(--lg-s7)',
        paddingY: 'var(--lg-s3)',
        gap: 'var(--lg-s3)',
        label: { fontWeight: 'var(--lg-fw-m)' },
      },
    },
    datepicker: {
      panel: {
        background: 'var(--surface)',
        borderColor: 'var(--lg-line)',
        borderRadius: 'var(--lg-r-3xl)',
        shadow: 'var(--lg-sh-4)',
        padding: 'var(--lg-s7)',
        color: 'var(--text)',
      },
      header: {
        background: 'transparent',
        borderColor: 'var(--lg-line)',
        padding: '0 0 var(--lg-s5)',
      },
      date: { width: 'calc(var(--k) * 36px)', height: 'calc(var(--k) * 36px)' },
      timePicker: { padding: 'var(--lg-s5) 0 0', gap: 'var(--lg-s5)' },
    },
    dialog: {
      header: { padding: 'var(--lg-s12) var(--lg-s12) var(--lg-s10)' },
      content: { padding: '0 var(--lg-s12) var(--lg-s12)' },
      title: { fontSize: 'var(--lg-fs-xl)', fontWeight: 'var(--lg-fw-b)' },
    },
    datatable: {
      headerCell: { padding: 'var(--lg-s6) var(--lg-s9)' },
      bodyCell: { padding: 'var(--lg-s7) var(--lg-s9)' },
      colorScheme: {
        light: {
          headerCell: { background: 'var(--lg-glass-2)', color: 'var(--muted)' },
          row: { background: 'transparent', color: 'var(--text)' },
        },
        dark: {
          root: { borderColor: 'var(--lg-line)' },
          headerCell: { background: 'var(--lg-glass-2)', color: 'var(--muted)' },
          row: { background: 'transparent', color: 'var(--text)' },
        },
      },
    },
    paginator: {
      root: { padding: '0', gap: 'var(--lg-s3)' },
      colorScheme: {
        light: { root: { background: 'transparent', color: 'var(--muted)' } },
        dark: { root: { background: 'transparent', color: 'var(--muted)' } },
      },
    },
  },
})
