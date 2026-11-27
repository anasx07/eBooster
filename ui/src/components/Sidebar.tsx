import React from 'react';
import { 
  LayoutDashboard, 
  Gamepad2, 
  Globe2, 
  Zap, 
  Settings as SettingsIcon,
  ShieldCheck,
  FolderOpen
} from 'lucide-react';
import { sendCommand } from '../api';

export type NavTab = 'dashboard' | 'profiles' | 'radar' | 'optimizer' | 'settings';

interface SidebarProps {
  currentTab: NavTab;
  onSelectTab: (tab: NavTab) => void;
  version: string;
}

export const Sidebar: React.FC<SidebarProps> = ({ currentTab, onSelectTab, version }) => {
  const navItems = [
    { id: 'dashboard', label: 'Command Center', icon: LayoutDashboard },
    { id: 'profiles', label: 'Game Profiles', icon: Gamepad2 },
    { id: 'radar', label: 'Dedicated Servers', icon: Globe2 },
    { id: 'optimizer', label: 'Kernel Optimizer', icon: Zap },
    { id: 'settings', label: 'Engine Settings', icon: SettingsIcon },
  ] as const;

  return (
    <aside className="w-56 bg-midnight-950/60 backdrop-blur-md border-r border-stadium-blue/15 flex flex-col justify-between py-4 select-none">
      <nav className="flex flex-col gap-1 px-3">
        {navItems.map(item => {
          const Icon = item.icon;
          const isActive = currentTab === item.id;
          return (
            <button
              key={item.id}
              onClick={() => onSelectTab(item.id)}
              className={`flex items-center gap-3 px-3.5 py-2.5 rounded-lg text-sm font-medium transition-all duration-200 text-left ${
                isActive
                  ? 'bg-stadium-blue/15 text-stadium-blue border border-stadium-blue/40 shadow-glow-blue'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/40'
              }`}
            >
              <Icon className={`w-4 h-4 ${isActive ? 'text-stadium-blue' : 'text-slate-400'}`} />
              <span>{item.label}</span>
            </button>
          );
        })}
      </nav>

      {/* Footer Info & Quick Actions */}
      <div className="px-4 flex flex-col gap-3">
        <button
          onClick={() => sendCommand('open_config_folder')}
          className="flex items-center gap-2 text-xs text-slate-400 hover:text-stadium-blue transition-colors py-1 px-2 rounded hover:bg-slate-800/50"
        >
          <FolderOpen className="w-3.5 h-3.5" />
          <span>Open Data Folder</span>
        </button>

        <div className="pt-3 border-t border-slate-800/80 flex items-center justify-between text-[11px] text-slate-500 font-mono">
          <div className="flex items-center gap-1.5">
            <ShieldCheck className="w-3.5 h-3.5 text-neon-green" />
            <span>WFP Kernel Safe</span>
          </div>
          <span>{version}</span>
        </div>
      </div>
    </aside>
  );
};
