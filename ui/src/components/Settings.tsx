import React from 'react';
import { 
  Settings as SettingsIcon, 
  FolderOpen, 
  RefreshCw, 
  RotateCcw, 
  Sliders, 
  HardDrive
} from 'lucide-react';
import { AppState } from '../types';
import { sendCommand } from '../api';

interface SettingsProps {
  state: AppState;
}

export const Settings: React.FC<SettingsProps> = ({ state }) => {
  const prefs = state.prefs || {
    autoBoostOnLaunch: true,
    autoStopOnExit: true,
    startWithWindows: false,
    minimizeToTray: true,
    closeToTray: true,
    overlayEnabled: false,
    efootballAntiLagShield: true,
    efootballAutoLock: false,
    selectedKonamiRegion: 1,
    storagePath: 'C:\\Users\\User\\AppData\\Local\\eBooster',
    isPortable: false
  };

  const handleToggle = (key: string, currentValue: boolean) => {
    sendCommand('set_setting', { key, value: !currentValue });
  };

  const handleOpenConfigFolder = () => {
    sendCommand('open_config_folder');
  };

  const handleCheckUpdates = () => {
    sendCommand('check_updates');
  };

  const handleResetSettings = () => {
    if (confirm('Reset all eBooster settings to stock defaults?')) {
      sendCommand('reset_settings');
    }
  };

  const toggleOptions = [
    {
      key: 'startWithWindows',
      title: 'Start with Windows (Logon Task)',
      desc: 'Starts eBooster minimized in the background via Windows Task Scheduler with highest gaming privileges.',
      value: prefs.startWithWindows,
    },
    {
      key: 'autoBoostOnLaunch',
      title: 'Auto-Boost on Game Launch',
      desc: 'Automatically engages WFP network filters as soon as your game executable is detected.',
      value: prefs.autoBoostOnLaunch,
    },
    {
      key: 'autoStopOnExit',
      title: 'Auto-Stop on Game Exit',
      desc: 'Reverts filters to standard Windows state immediately when the game closes.',
      value: prefs.autoStopOnExit,
    },
    {
      key: 'minimizeToTray',
      title: 'Minimize to System Tray',
      desc: 'Reduces eBooster to the Windows notification tray instead of taking taskbar space.',
      value: prefs.minimizeToTray,
    },
    {
      key: 'closeToTray',
      title: 'Close Button Minimizes to Tray',
      desc: 'Clicking the (X) button keeps the low-latency kernel shield running in the background.',
      value: prefs.closeToTray,
    },
    {
      key: 'overlayEnabled',
      title: 'In-Game Telemetry Mini HUD Overlay',
      desc: 'Displays a transparent, ultra-lightweight ping and packet loss HUD on top of your game window.',
      value: prefs.overlayEnabled,
    },
    {
      key: 'efootballAntiLagShield',
      title: 'eFootball Anti-TCP Lag Shield',
      desc: 'Blocks outbound TCP 30000-35000 and 5736 to guarantee pure dedicated UDP transport.',
      value: prefs.efootballAntiLagShield,
    }
  ];

  return (
    <div className="flex-1 overflow-y-auto p-6 space-y-6">
      {/* Header Banner */}
      <div className="glass-panel rounded-2xl p-6 relative overflow-hidden flex flex-col md:flex-row items-center justify-between gap-4">
        <div className="space-y-1">
          <div className="flex items-center gap-2">
            <SettingsIcon className="w-4 h-4 text-stadium-blue" />
            <span className="text-xs font-mono font-semibold text-stadium-blue uppercase tracking-wider">
              System & Engine Configuration
            </span>
          </div>
          <h2 className="text-2xl font-heading font-bold text-white">
            Preferences & Application Settings
          </h2>
          <p className="text-xs text-slate-400 max-w-xl">
            Configure system startup behavior, tray docking, overlay telemetry, and file paths.
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={handleCheckUpdates}
            className="flex items-center gap-2 px-4 py-2.5 rounded-xl bg-midnight-800 hover:bg-midnight-700 text-xs font-semibold text-slate-300 border border-slate-700 transition-colors"
          >
            <RefreshCw className="w-3.5 h-3.5 text-stadium-blue" />
            <span>CHECK FOR UPDATES</span>
          </button>
        </div>
      </div>

      {/* Storage & Environment Info */}
      <div className="glass-panel rounded-2xl p-5 border border-slate-800 flex flex-col sm:flex-row items-center justify-between gap-4 text-xs font-mono">
        <div className="flex items-center gap-3">
          <div className="p-2 rounded-lg bg-stadium-blue/15 text-stadium-blue">
            <HardDrive className="w-4 h-4" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <span className="text-white font-semibold">Active Storage Path:</span>
              <span className={`text-[10px] px-2 py-0.5 rounded font-bold ${
                prefs.isPortable 
                  ? 'bg-amber-500/20 text-amber-400 border border-amber-500/40' 
                  : 'bg-stadium-blue/20 text-stadium-blue border border-stadium-blue/40'
              }`}>
                {prefs.isPortable ? 'PORTABLE (USB) MODE' : 'STANDARD LOCALAPPDATA'}
              </span>
            </div>
            <p className="text-slate-400 text-[11px] mt-0.5 break-all">{prefs.storagePath}</p>
          </div>
        </div>

        <button
          onClick={handleOpenConfigFolder}
          className="flex items-center gap-2 px-3.5 py-2 rounded-lg bg-midnight-800 hover:bg-slate-800 text-slate-300 border border-slate-700 transition-colors text-xs shrink-0"
        >
          <FolderOpen className="w-3.5 h-3.5 text-stadium-blue" />
          <span>Open Folder</span>
        </button>
      </div>

      {/* Toggles List */}
      <div className="glass-panel rounded-2xl p-6 border border-slate-800 divide-y divide-slate-800/80">
        <h3 className="font-heading font-bold text-base text-white pb-4 flex items-center gap-2">
          <Sliders className="w-4 h-4 text-stadium-blue" />
          <span>General Preferences</span>
        </h3>

        {toggleOptions.map(opt => (
          <div 
            key={opt.key}
            onClick={() => handleToggle(opt.key, opt.value)}
            className="py-4 flex items-center justify-between cursor-pointer group hover:bg-slate-800/20 px-2 rounded-lg transition-colors"
          >
            <div className="space-y-1 pr-4">
              <h4 className="text-sm font-semibold text-white group-hover:text-stadium-blue transition-colors">
                {opt.title}
              </h4>
              <p className="text-xs text-slate-400">{opt.desc}</p>
            </div>

            {/* iOS/Modern style Switch */}
            <div className={`w-12 h-6 rounded-full transition-colors duration-200 relative shrink-0 p-1 ${
              opt.value ? 'bg-stadium-blue shadow-glow-blue' : 'bg-slate-800'
            }`}>
              <div className={`w-4 h-4 rounded-full bg-white transition-transform duration-200 ${
                opt.value ? 'translate-x-6' : 'translate-x-0'
              }`} />
            </div>
          </div>
        ))}
      </div>

      {/* Danger / Reset Area */}
      <div className="pt-2 flex items-center justify-between text-xs">
        <button
          onClick={handleResetSettings}
          className="flex items-center gap-2 text-slate-500 hover:text-red-400 transition-colors"
        >
          <RotateCcw className="w-3.5 h-3.5" />
          <span>Reset All Configuration to Factory Defaults</span>
        </button>
        <span className="text-slate-600 font-mono">eBooster Pro v1.0.4 • 64-bit Native C++23</span>
      </div>
    </div>
  );
};
