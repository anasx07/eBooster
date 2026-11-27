import React, { useState } from 'react';
import { 
  Gamepad2, 
  Plus, 
  Trash2, 
  CheckCircle2, 
  X
} from 'lucide-react';
import { AppState } from '../types';
import { sendCommand } from '../api';

interface GameProfilesProps {
  state: AppState;
}

export const GameProfiles: React.FC<GameProfilesProps> = ({ state }) => {
  const [showAddModal, setShowAddModal] = useState(false);
  const [newGame, setNewGame] = useState({
    name: '',
    exeName: '',
    defaultPortHint: 'UDP 30000-35000',
    steamAppId: 0,
    clusterCidrs: ''
  });

  const handleSelectProfile = (idx: number) => {
    sendCommand('select_profile', { index: idx });
  };

  const handleDeleteProfile = (e: React.MouseEvent, idx: number) => {
    e.stopPropagation();
    if (confirm('Are you sure you want to delete this custom game profile?')) {
      sendCommand('delete_profile', { index: idx });
    }
  };

  const handleCreateProfile = (e: React.FormEvent) => {
    e.preventDefault();
    if (!newGame.name || !newGame.exeName) return;

    const cidrs = newGame.clusterCidrs
      .split('\n')
      .map(s => s.trim())
      .filter(s => s.length > 0);

    sendCommand('add_custom_profile', {
      name: newGame.name,
      exeName: newGame.exeName,
      defaultPortHint: newGame.defaultPortHint,
      steamAppId: Number(newGame.steamAppId) || 0,
      clusterCidrs: cidrs
    });

    setShowAddModal(false);
    setNewGame({
      name: '',
      exeName: '',
      defaultPortHint: 'UDP 30000-35000',
      steamAppId: 0,
      clusterCidrs: ''
    });
  };

  return (
    <div className="flex-1 overflow-y-auto p-6 space-y-6">
      {/* Header Banner */}
      <div className="glass-panel rounded-2xl p-6 relative overflow-hidden flex flex-col md:flex-row items-center justify-between gap-4">
        <div className="space-y-1">
          <div className="flex items-center gap-2">
            <Gamepad2 className="w-4 h-4 text-stadium-blue" />
            <span className="text-xs font-mono font-semibold text-stadium-blue uppercase tracking-wider">
              Esports Game Profiles Library
            </span>
          </div>
          <h2 className="text-2xl font-heading font-bold text-white">
            Configured Gaming Profiles & Routing Filters
          </h2>
          <p className="text-xs text-slate-400 max-w-xl">
            Select an active game profile to automatically apply low-latency WFP port filters, 
            packet priority rules, and dedicated server IP locks.
          </p>
        </div>

        <button
          onClick={() => setShowAddModal(true)}
          className="flex items-center gap-2 px-4 py-2.5 rounded-xl bg-stadium-blue hover:bg-cyan-400 text-white font-semibold text-xs tracking-wide shadow-glow-blue transition-all"
        >
          <Plus className="w-4 h-4" />
          <span>ADD CUSTOM GAME</span>
        </button>
      </div>

      {/* Profiles Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-5">
        {state.profiles.map((profile, idx) => {
          const isSelected = state.selectedProfileIdx === idx;
          return (
            <div
              key={`${profile.name}-${idx}`}
              onClick={() => handleSelectProfile(idx)}
              className={`glass-panel rounded-2xl p-5 cursor-pointer transition-all duration-300 relative overflow-hidden flex flex-col justify-between group ${
                isSelected 
                  ? 'border-stadium-blue bg-stadium-blue/10 shadow-glow-blue' 
                  : 'hover:border-slate-600 hover:bg-slate-800/40'
              }`}
            >
              {/* Active Badge */}
              <div className="flex items-start justify-between">
                <div>
                  <span className={`text-[10px] font-mono px-2 py-0.5 rounded uppercase font-semibold ${
                    isSelected 
                      ? 'bg-stadium-blue/20 text-stadium-blue border border-stadium-blue/50' 
                      : 'bg-slate-800 text-slate-400'
                  }`}>
                    {isSelected ? 'ACTIVE GAME' : 'STANDBY'}
                  </span>
                  <h3 className="font-heading font-bold text-lg text-white mt-2 group-hover:text-stadium-blue transition-colors flex items-center gap-2">
                    <span>{profile.name}</span>
                    {isSelected && <CheckCircle2 className="w-4 h-4 text-stadium-blue" />}
                  </h3>
                </div>

                {profile.isCustom && (
                  <button
                    onClick={(e) => handleDeleteProfile(e, idx)}
                    className="p-1.5 rounded-lg text-slate-500 hover:text-red-400 hover:bg-red-500/10 transition-colors"
                    title="Delete Custom Profile"
                  >
                    <Trash2 className="w-3.5 h-3.5" />
                  </button>
                )}
              </div>

              {/* Specs & Info */}
              <div className="my-4 space-y-2 text-xs font-mono">
                <div className="flex items-center justify-between text-slate-400">
                  <span className="text-slate-500">Executable:</span>
                  <span className="text-slate-300 font-semibold">{profile.exeName}</span>
                </div>
                <div className="flex items-center justify-between text-slate-400">
                  <span className="text-slate-500">Port Filter:</span>
                  <span className="text-slate-300">{profile.defaultPortHint || 'Auto UDP'}</span>
                </div>
                <div className="flex items-center justify-between text-slate-400">
                  <span className="text-slate-500">Clusters:</span>
                  <span className="text-stadium-blue font-semibold">
                    {profile.clusterCidrs?.length || 0} CIDR subnets
                  </span>
                </div>
              </div>

              {/* Footer action button */}
              <div className="pt-3 border-t border-slate-800/80 flex items-center justify-between text-xs">
                <span className="text-slate-500 text-[11px]">
                  {profile.isCustom ? 'Custom User Profile' : 'Verified Official Preset'}
                </span>
                <span className={`font-semibold ${isSelected ? 'text-stadium-blue' : 'text-slate-400 group-hover:text-white'}`}>
                  {isSelected ? 'Engaged' : 'Select'}
                </span>
              </div>
            </div>
          );
        })}
      </div>

      {/* Add Custom Game Modal */}
      {showAddModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/70 backdrop-blur-sm animate-fade-in">
          <div className="glass-panel w-full max-w-lg rounded-2xl p-6 relative border border-stadium-blue/30 shadow-2xl space-y-5">
            <div className="flex items-center justify-between border-b border-slate-800 pb-3">
              <div className="flex items-center gap-2">
                <Gamepad2 className="w-5 h-5 text-stadium-blue" />
                <h3 className="font-heading font-bold text-lg text-white">Add Custom Game Profile</h3>
              </div>
              <button 
                onClick={() => setShowAddModal(false)}
                className="text-slate-400 hover:text-white p-1 rounded hover:bg-slate-800"
              >
                <X className="w-4 h-4" />
              </button>
            </div>

            <form onSubmit={handleCreateProfile} className="space-y-4 text-xs">
              <div>
                <label className="block text-slate-400 font-mono mb-1">Game Name</label>
                <input
                  type="text"
                  required
                  placeholder="e.g. Valorant / Apex Legends"
                  value={newGame.name}
                  onChange={e => setNewGame({ ...newGame, name: e.target.value })}
                  className="w-full px-3 py-2 rounded-lg bg-midnight-900 border border-slate-700 text-white placeholder-slate-600 focus:outline-none focus:border-stadium-blue"
                />
              </div>

              <div>
                <label className="block text-slate-400 font-mono mb-1">Executable Name (e.g. game.exe)</label>
                <input
                  type="text"
                  required
                  placeholder="e.g. r5apex.exe"
                  value={newGame.exeName}
                  onChange={e => setNewGame({ ...newGame, exeName: e.target.value })}
                  className="w-full px-3 py-2 rounded-lg bg-midnight-900 border border-slate-700 text-white placeholder-slate-600 focus:outline-none focus:border-stadium-blue"
                />
              </div>

              <div className="grid grid-cols-2 gap-3">
                <div>
                  <label className="block text-slate-400 font-mono mb-1">Default Port / Range</label>
                  <input
                    type="text"
                    placeholder="e.g. UDP 30000-35000"
                    value={newGame.defaultPortHint}
                    onChange={e => setNewGame({ ...newGame, defaultPortHint: e.target.value })}
                    className="w-full px-3 py-2 rounded-lg bg-midnight-900 border border-slate-700 text-white placeholder-slate-600 focus:outline-none focus:border-stadium-blue"
                  />
                </div>
                <div>
                  <label className="block text-slate-400 font-mono mb-1">Steam App ID (Optional)</label>
                  <input
                    type="number"
                    placeholder="e.g. 730"
                    value={newGame.steamAppId || ''}
                    onChange={e => setNewGame({ ...newGame, steamAppId: parseInt(e.target.value) || 0 })}
                    className="w-full px-3 py-2 rounded-lg bg-midnight-900 border border-slate-700 text-white placeholder-slate-600 focus:outline-none focus:border-stadium-blue"
                  />
                </div>
              </div>

              <div>
                <label className="block text-slate-400 font-mono mb-1">
                  Dedicated Server CIDR Subnets (One per line)
                </label>
                <textarea
                  rows={3}
                  placeholder="155.133.248.0/24&#10;185.60.112.0/24"
                  value={newGame.clusterCidrs}
                  onChange={e => setNewGame({ ...newGame, clusterCidrs: e.target.value })}
                  className="w-full px-3 py-2 rounded-lg bg-midnight-900 border border-slate-700 text-white placeholder-slate-600 focus:outline-none focus:border-stadium-blue font-mono text-[11px]"
                />
              </div>

              <div className="pt-3 flex items-center justify-end gap-3">
                <button
                  type="button"
                  onClick={() => setShowAddModal(false)}
                  className="px-4 py-2 rounded-lg bg-midnight-800 text-slate-300 hover:bg-slate-800"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  className="px-5 py-2 rounded-lg bg-stadium-blue hover:bg-cyan-400 text-white font-semibold shadow-glow-blue"
                >
                  Save Profile
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};
