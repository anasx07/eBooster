/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        midnight: {
          950: '#05070D',
          900: '#080B14',
          850: '#0D121F',
          800: '#111827',
          700: '#1E293B',
        },
        stadium: {
          blue: '#00A3FF',
          dark: '#0070B8',
          glow: 'rgba(0, 163, 255, 0.35)',
        },
        neon: {
          magenta: '#FF007F',
          cyan: '#00F0FF',
          green: '#10B981',
          amber: '#F59E0B',
          red: '#EF4444',
        }
      },
      boxShadow: {
        'glow-blue': '0 0 20px rgba(0, 163, 255, 0.35)',
        'glow-magenta': '0 0 20px rgba(255, 0, 127, 0.35)',
        'glow-green': '0 0 20px rgba(16, 185, 129, 0.35)',
        'card': '0 8px 32px 0 rgba(0, 0, 0, 0.45)',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'radar-sweep': 'radarSweep 4s linear infinite',
      },
      keyframes: {
        radarSweep: {
          '0%': { transform: 'rotate(0deg)' },
          '100%': { transform: 'rotate(360deg)' },
        }
      }
    },
  },
  plugins: [],
}
