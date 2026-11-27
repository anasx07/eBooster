import React, { useState, useEffect } from 'react';
import { TopBar } from './components/TopBar';
import { Sidebar, NavTab } from './components/Sidebar';
import { Dashboard } from './components/Dashboard';
import { GameProfiles } from './components/GameProfiles';
import { ServerRadar } from './components/ServerRadar';
import { Optimizer } from './components/Optimizer';
import { Settings } from './components/Settings';
import { AppState } from './types';
import { onNativeMessage, sendCommand } from './api';

// Default mock state for standalone Vite preview & development fallback
const defaultState: AppState = {
  version: 'v1.0.4',
  isBoosting: false,
  selectedProfileIdx: 0,
  currentPing: 24,
  baselineIspPing: 48,
  packetLossPct: 0.0,
  jitterMs: 1.2,
  profiles: [
    {
      name: 'eFootball (Steam)',
      exeName: 'eFootball.exe',
      childExes: [],
      launcherExes: [],
      defaultPortHint: 'UDP 30000-35000',
      clusterCidrs: ['155.133.248.0/24', '185.60.112.0/24', '185.60.114.0/24'],
      isCustom: false,
      steamAppId: 1665460,
      bannerPath: '',
      logoPath: ''
    },
    {
      name: 'Counter-Strike 2',
      exeName: 'cs2.exe',
      childExes: [],
      launcherExes: [],
      defaultPortHint: 'UDP 27015-27050',
      clusterCidrs: ['155.133.248.0/24', '155.133.246.0/24'],
      isCustom: false,
      steamAppId: 730,
      bannerPath: '',
      logoPath: ''
    },
    {
      name: 'Apex Legends',
      exeName: 'r5apex.exe',
      childExes: [],
      launcherExes: [],
      defaultPortHint: 'UDP 37015-37020',
      clusterCidrs: ['155.133.240.0/24'],
      isCustom: false,
      steamAppId: 1172470,
      bannerPath: '',
      logoPath: ''
    },
    {
      name: 'Valorant',
      exeName: 'VALORANT-Win64-Shipping.exe',
      childExes: ['RiotClientServices.exe'],
      launcherExes: [],
      defaultPortHint: 'UDP 7000-8000',
      clusterCidrs: ['162.249.72.0/24'],
      isCustom: false,
      steamAppId: 0,
      bannerPath: '',
      logoPath: ''
    },
    {
      name: 'Rocket League',
      exeName: 'RocketLeague.exe',
      childExes: [],
      launcherExes: [],
      defaultPortHint: 'UDP 7000-9000',
      clusterCidrs: ['155.133.242.0/24'],
      isCustom: false,
      steamAppId: 252950,
      bannerPath: '',
      logoPath: ''
    }
  ],
  optimizer: {
    nagleDisabled: true,
    throttlingDisabled: true,
    responsivenessOptimized: true,
    gamingPriorityElevated: true,
    mpoDisabled: true,
    efootballAntiLagShield: true,
    optimizedAdaptersCount: 1
  },
  matchInfo: {
    isMatchActive: false,
    livePingMs: 24,
    locationName: 'Frankfurt, Germany',
    remoteIp: '155.133.248.51',
    serverRegion: 'EU-Central',
    packetRate: 120,
    protocol: 'UDP'
  },
  prefs: {
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
  }
};

export const App: React.FC = () => {
  const [currentTab, setCurrentTab] = useState<NavTab>('dashboard');
  const [state, setState] = useState<AppState>(defaultState);

  useEffect(() => {
    // Request initial state from C++ backend upon load
    sendCommand('get_state');

    const cleanup = onNativeMessage((msg) => {
      if (!msg || !msg.type) return;

      if (msg.type === 'state_update') {
        setState(prev => ({
          ...prev,
          ...msg.payload
        }));
      } else if (msg.type === 'telemetry_update') {
        setState(prev => ({
          ...prev,
          currentPing: msg.payload.currentPing ?? prev.currentPing,
          baselineIspPing: msg.payload.baselineIspPing ?? prev.baselineIspPing,
          packetLossPct: msg.payload.packetLossPct ?? prev.packetLossPct,
          jitterMs: msg.payload.jitterMs ?? prev.jitterMs,
          matchInfo: {
            ...prev.matchInfo,
            ...msg.payload.matchInfo
          }
        }));
      }
    });

    return cleanup;
  }, []);

  const activeGame = state.profiles[state.selectedProfileIdx]?.name || 'eFootball';

  return (
    <div className="flex flex-col h-screen w-screen bg-midnight-950 text-slate-100 overflow-hidden font-sans">
      {/* Frameless Win32 Titlebar & Controls */}
      <TopBar isBoosting={state.isBoosting} activeGame={activeGame} />

      {/* Main Workspace */}
      <div className="flex flex-1 overflow-hidden relative">
        <Sidebar 
          currentTab={currentTab} 
          onSelectTab={setCurrentTab} 
          version={state.version} 
        />

        <main className="flex-1 flex overflow-hidden bg-radial-gradient">
          {currentTab === 'dashboard' && (
            <Dashboard state={state} onSelectTab={setCurrentTab} />
          )}
          {currentTab === 'profiles' && (
            <GameProfiles state={state} />
          )}
          {currentTab === 'radar' && (
            <ServerRadar state={state} />
          )}
          {currentTab === 'optimizer' && (
            <Optimizer state={state} />
          )}
          {currentTab === 'settings' && (
            <Settings state={state} />
          )}
        </main>
      </div>
    </div>
  );
};
