export interface GameProfile {
  name: string;
  exeName: string;
  childExes: string[];
  launcherExes: string[];
  defaultPortHint: string;
  clusterCidrs: string[];
  isCustom: boolean;
  steamAppId: number;
  bannerPath: string;
  logoPath: string;
}

export interface OptimizerStatus {
  nagleDisabled: boolean;
  throttlingDisabled: boolean;
  responsivenessOptimized: boolean;
  gamingPriorityElevated: boolean;
  mpoDisabled: boolean;
  efootballAntiLagShield: boolean;
  optimizedAdaptersCount: number;
}

export interface MatchInfo {
  isMatchActive: boolean;
  livePingMs: number;
  locationName: string;
  remoteIp: string;
  serverRegion: string;
  packetRate: number;
  protocol: string;
}

export interface AppPreferences {
  autoBoostOnLaunch: boolean;
  autoStopOnExit: boolean;
  startWithWindows: boolean;
  minimizeToTray: boolean;
  closeToTray: boolean;
  overlayEnabled: boolean;
  efootballAntiLagShield: boolean;
  efootballAutoLock: boolean;
  selectedKonamiRegion: number;
  storagePath: string;
  isPortable: boolean;
}

export interface KonamiRegionNode {
  regionCode: number;
  name: string;
  country: string;
  desc: string;
  ip: string;
  ping: number;
}

export interface UpdateStatusInfo {
  checking: boolean;
  available: boolean;
  latestVersion: string;
  message: string;
}

export interface AppState {
  version: string;
  isBoosting: boolean;
  selectedProfileIdx: number;
  profiles: GameProfile[];
  konamiRegions?: KonamiRegionNode[];
  currentPing: number;
  baselineIspPing: number;
  packetLossPct: number;
  jitterMs: number;
  optimizer: OptimizerStatus;
  matchInfo: MatchInfo;
  prefs: AppPreferences;
  updateStatus?: UpdateStatusInfo;
}
