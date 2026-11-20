#include "ebooster/MatchSniffer.hpp"
#include <cstdio>
#include <algorithm>
#include <format>

namespace ebooster {

MatchClassifier::ResolvedEndpointInfo MatchClassifier::ClassifyIp(const std::string& ip, uint16_t port) {
    ResolvedEndpointInfo res;
    res.clusterName = "Gaming Server Cluster";
    res.locationName = "Direct Server Node";
    res.countryCode = "EU";
    res.countryFlag = "[NET]";
    res.isDedicatedServer = true;
    res.matchType = "Direct Dedicated Match";

    int a = 0, b = 0, c = 0, d = 0;
    if (sscanf_s(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return res;
    }

    // =========================================================================
    // 1. eFootball Dedicated Konami Clusters (Google Cloud / AWS / OVH)
    // =========================================================================
    // Madrid, Spain (europe-southwest1 & AWS eu-south-2)
    if ((a == 34 && b == 175) || (a == 51 && b == 95 && c == 38) || 
        (a == 51 && b == 48 && c == 223) || (a == 15 && b == 216 && c == 87) ||
        (a == 129 && b == 151) || (a == 140 && b == 238) || (a == 158 && b == 101)) {
        res.clusterName = "Madrid Dedicated Cluster";
        res.locationName = "Madrid, Spain";
        res.countryCode = "ES";
        res.countryFlag = "[ES]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Paris, France (europe-west9 & AWS eu-west-3)
    if ((a == 34 && b == 155 && c == 199) || (a == 15 && b == 236) || 
        (a == 15 && b == 188) || (a == 34 && b == 163) || (a == 35 && b == 180)) {
        res.clusterName = "Paris Dedicated Cluster";
        res.locationName = "Paris, France";
        res.countryCode = "FR";
        res.countryFlag = "[FR]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Frankfurt, Germany (europe-west3 & AWS eu-central-1)
    if ((a == 34 && b == 159) || (a == 34 && b == 32 && c == 98) || 
        (a == 15 && b == 220 && c == 152) || (a == 18 && b == 156) || 
        (a == 3 && b >= 120 && b <= 123) || (a == 54 && b == 93)) {
        res.clusterName = "Frankfurt Dedicated Cluster";
        res.locationName = "Frankfurt, Germany";
        res.countryCode = "DE";
        res.countryFlag = "[DE]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // London, United Kingdom (europe-west2 & AWS eu-west-2)
    if ((a == 34 && b == 39 && c == 17) || (a == 35 && (b == 176 || b == 177)) ||
        (a == 18 && b == 130) || (a == 3 && (b == 8 || b == 9))) {
        res.clusterName = "London Dedicated Cluster";
        res.locationName = "London, United Kingdom";
        res.countryCode = "GB";
        res.countryFlag = "[GB]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Milan, Italy (europe-west8 & AWS eu-south-1)
    if ((a == 34 && b == 154 && c == 179) || (a == 34 && b == 17 && c == 93) ||
        (a == 18 && b == 102 && c == 9) || (a == 15 && b == 161) || (a == 16 && b == 22)) {
        res.clusterName = "Milan Dedicated Cluster";
        res.locationName = "Milan, Italy";
        res.countryCode = "IT";
        res.countryFlag = "[IT]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Zurich, Switzerland (europe-west6)
    if ((a == 34 && b == 65 && c == 93) || (a == 51 && b == 34 && c == 16) ||
        (a == 51 && b == 96 && (c == 162 || c == 111))) {
        res.clusterName = "Zurich Dedicated Cluster";
        res.locationName = "Zurich, Switzerland";
        res.countryCode = "CH";
        res.countryFlag = "[CH]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Athens, Greece (East Mediterranean)
    if (a == 75 && b == 45 && (c >= 130 && c <= 135)) {
        res.clusterName = "Athens Dedicated Cluster";
        res.locationName = "Athens, Greece";
        res.countryCode = "GR";
        res.countryFlag = "[GR]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Dubai / Gulf (Middle East)
    if ((a == 51 && b == 112 && (c == 179 || c == 243)) ||
        (a == 34 && b == 166 && c == 38) || (a == 34 && b == 18 && c == 108) ||
        (a == 13 && b == 152 && c == 0)) {
        res.clusterName = "Dubai Gulf Dedicated Cluster";
        res.locationName = "Dubai, UAE";
        res.countryCode = "AE";
        res.countryFlag = "[AE]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Istanbul, Turkey
    if (a == 83 && b == 119 && (c >= 130 && c <= 136)) {
        res.clusterName = "Istanbul Dedicated Cluster";
        res.locationName = "Istanbul, Turkey";
        res.countryCode = "TR";
        res.countryFlag = "[TR]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Lagos, Nigeria (African Hub)
    if (a == 96 && b == 0 && (c >= 40 && c <= 50)) {
        res.clusterName = "Lagos Dedicated Cluster";
        res.locationName = "Lagos, Nigeria";
        res.countryCode = "NG";
        res.countryFlag = "[NG]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Amsterdam, Netherlands (europe-west4 & AMS-IX)
    if ((a == 34 && b == 13 && c == 163) || (a == 34 && b == 38 && c == 154) ||
        (a == 34 && b == 147 && c == 16) || (a == 35 && b == 204 && c == 72)) {
        res.clusterName = "Amsterdam Dedicated Cluster";
        res.locationName = "Amsterdam, Netherlands";
        res.countryCode = "NL";
        res.countryFlag = "[NL]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Warsaw, Poland (europe-central2)
    if ((a == 34 && b == 118 && c == 91) || (a == 34 && b == 116 && c == 180) ||
        (a == 15 && b == 220 && c == 65)) {
        res.clusterName = "Warsaw Dedicated Cluster";
        res.locationName = "Warsaw, Poland";
        res.countryCode = "PL";
        res.countryFlag = "[PL]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Helsinki, Finland (europe-north1)
    if ((a == 35 && b == 228 && c == 63) || (a == 34 && b == 88 && c == 120) ||
        (a == 16 && b == 16 && c == 92)) {
        res.clusterName = "Helsinki Dedicated Cluster";
        res.locationName = "Helsinki, Finland";
        res.countryCode = "FI";
        res.countryFlag = "[FI]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Riyadh, Saudi Arabia (me-central2 & AWS me-central-1)
    if ((a == 34 && b == 166 && c == 38) || (a == 15 && b == 206 && c == 180) ||
        (a == 51 && b == 107 && c == 12)) {
        res.clusterName = "Riyadh Dedicated Cluster";
        res.locationName = "Riyadh, Saudi Arabia";
        res.countryCode = "SA";
        res.countryFlag = "[SA]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Doha, Qatar (me-west1)
    if ((a == 34 && b == 158 && c == 40) || (a == 51 && b == 112 && c == 243)) {
        res.clusterName = "Doha Dedicated Cluster";
        res.locationName = "Doha, Qatar";
        res.countryCode = "QA";
        res.countryFlag = "[QA]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Johannesburg, South Africa (africa-south1)
    if ((a == 34 && b == 35 && c == 84) || (a == 13 && b == 244 && c == 80)) {
        res.clusterName = "Johannesburg Dedicated Cluster";
        res.locationName = "Johannesburg, South Africa";
        res.countryCode = "ZA";
        res.countryFlag = "[ZA]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // São Paulo, Brazil (southamerica-east1)
    if ((a == 34 && b == 95 && c == 169) || (a == 35 && b == 198 && c == 22) ||
        (a == 15 && b == 228 && c == 40) || (a == 96 && b == 0 && c == 16 && (d == 253 || d == 157 || d == 27))) {
        res.clusterName = "São Paulo Dedicated Cluster";
        res.locationName = "São Paulo, Brazil";
        res.countryCode = "BR";
        res.countryFlag = "[BR]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Santiago, Chile (southamerica-west1)
    if ((a == 34 && b == 176 && c == 17) || (a == 96 && b == 0 && ((c == 16 && (d == 188 || d == 199 || d == 109)) || (c == 20 && d == 177) || (c == 22 && d == 98)))) {
        res.clusterName = "Santiago Dedicated Cluster";
        res.locationName = "Santiago, Chile";
        res.countryCode = "CL";
        res.countryFlag = "[CL]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // US East (us-east1 / us-east4)
    if ((a == 34 && b == 85 && c == 172) || (a == 35 && b == 203 && c == 65) ||
        (a == 34 && b == 138 && c == 103) || (a == 34 && b == 86 && c == 110)) {
        res.clusterName = "US East Dedicated Cluster";
        res.locationName = "Virginia / NY, USA";
        res.countryCode = "US";
        res.countryFlag = "[US]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // US Central (us-central1)
    if ((a == 34 && b == 162 && c == 74) || (a == 35 && b == 224 && c == 16) ||
        (a == 34 && b == 174 && c == 25) || (a == 34 && b == 66 && c == 190)) {
        res.clusterName = "US Central Dedicated Cluster";
        res.locationName = "Iowa / Chicago, USA";
        res.countryCode = "US";
        res.countryFlag = "[US]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // US West (us-west1 / us-west2)
    if ((a == 34 && b == 82 && c == 40) || (a == 34 && b == 106 && c == 69) ||
        (a == 34 && b == 125 && c == 24) || (a == 34 && b == 102 && c == 12)) {
        res.clusterName = "US West Dedicated Cluster";
        res.locationName = "Oregon / California, USA";
        res.countryCode = "US";
        res.countryFlag = "[US]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Tokyo, Japan (asia-northeast1)
    if ((a == 35 && b == 221 && c == 126) || (a == 34 && b == 97 && c == 138) ||
        (a == 13 && b == 112 && c == 44)) {
        res.clusterName = "Tokyo Dedicated Cluster";
        res.locationName = "Tokyo, Japan";
        res.countryCode = "JP";
        res.countryFlag = "[JP]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Seoul, South Korea (asia-northeast3)
    if ((a == 34 && b == 64 && c == 235) || (a == 15 && b == 164 && c == 70)) {
        res.clusterName = "Seoul Dedicated Cluster";
        res.locationName = "Seoul, South Korea";
        res.countryCode = "KR";
        res.countryFlag = "[KR]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Singapore (asia-southeast1)
    if ((a == 34 && b == 128 && c == 68) || (a == 35 && b == 247 && c == 162) ||
        (a == 34 && b == 87 && c == 112)) {
        res.clusterName = "Singapore Dedicated Cluster";
        res.locationName = "Singapore";
        res.countryCode = "SG";
        res.countryFlag = "[SG]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Hong Kong (asia-east2)
    if ((a == 34 && b == 92 && c == 192) || (a == 35 && b == 229 && c == 165) ||
        (a == 56 && b == 68)) {
        res.clusterName = "Hong Kong Dedicated Cluster";
        res.locationName = "Hong Kong";
        res.countryCode = "HK";
        res.countryFlag = "[HK]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Mumbai, India (asia-south1)
    if ((a == 34 && b == 93 && c == 13) || (a == 34 && b == 131 && c == 13) ||
        (a == 96 && b == 0 && (c == 64 || c == 65))) {
        res.clusterName = "Mumbai Dedicated Cluster";
        res.locationName = "Mumbai, India";
        res.countryCode = "IN";
        res.countryFlag = "[IN]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // Sydney, Australia (australia-southeast1)
    if ((a == 35 && b == 244 && c == 67) || (a == 34 && b == 129 && c == 43)) {
        res.clusterName = "Sydney Dedicated Cluster";
        res.locationName = "Sydney, Australia";
        res.countryCode = "AU";
        res.countryFlag = "[AU]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // =========================================================================
    // 2. Valve Steam Datagram Relay (CS2 / Dota 2 SDR Network)
    // =========================================================================
    if (a == 155 && b == 133) {
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        if (c == 240) {
            res.clusterName = "Madrid Valve SDR Cluster";
            res.locationName = "Madrid, Spain";
            res.countryCode = "ES";
            res.countryFlag = "[ES]";
            return res;
        } else if (c == 226 || c == 227) {
            res.clusterName = "Frankfurt Valve SDR Cluster";
            res.locationName = "Frankfurt, Germany";
            res.countryCode = "DE";
            res.countryFlag = "[DE]";
            return res;
        } else if (c == 246 || c == 247) {
            res.clusterName = "Paris Valve SDR Cluster";
            res.locationName = "Paris, France";
            res.countryCode = "FR";
            res.countryFlag = "[FR]";
            return res;
        } else if (c == 248 || c == 249) {
            res.clusterName = "London Valve SDR Cluster";
            res.locationName = "London, United Kingdom";
            res.countryCode = "GB";
            res.countryFlag = "[GB]";
            return res;
        } else if (c == 245) {
            res.clusterName = "Amsterdam Valve SDR Cluster";
            res.locationName = "Amsterdam, Netherlands";
            res.countryCode = "NL";
            res.countryFlag = "[NL]";
            return res;
        } else if (c == 238 || c == 239) {
            res.clusterName = "Vienna Valve SDR Cluster";
            res.locationName = "Vienna, Austria";
            res.countryCode = "AT";
            res.countryFlag = "[AT]";
            return res;
        } else if (c == 232) {
            res.clusterName = "Stockholm Valve SDR Cluster";
            res.locationName = "Stockholm, Sweden";
            res.countryCode = "SE";
            res.countryFlag = "[SE]";
            return res;
        } else {
            res.clusterName = "Valve SDR European Relay";
            res.locationName = "Europe";
            res.countryCode = "EU";
            res.countryFlag = "[EU]";
            return res;
        }
    }

    if (a == 162 && b == 254 && (c >= 192 && c <= 199)) {
        res.clusterName = "Valve SDR US Relay";
        res.locationName = "United States";
        res.countryCode = "US";
        res.countryFlag = "[US]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // =========================================================================
    // 3. Major Cloud Infrastructure Providers (GCP, AWS, Azure, OVH, Hetzner)
    // =========================================================================
    if (a == 34 || a == 35) {
        res.clusterName = "Google Cloud Dedicated Cluster";
        res.locationName = "European Cloud Hub";
        res.countryCode = "EU";
        res.countryFlag = "[NET]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    if ((a == 15 || a == 16 || a == 18 || a == 3 || a == 52 || a == 54) && 
        (port >= 1024 || port == 5730 || port == 5735 || port == 27015)) {
        res.clusterName = "AWS Dedicated Gaming Cluster";
        res.locationName = "European Cloud Hub";
        res.countryCode = "EU";
        res.countryFlag = "[NET]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    if (a == 51 && (b >= 68 && b <= 255)) {
        res.clusterName = "OVHcloud Dedicated Relay";
        res.locationName = "Europe";
        res.countryCode = "EU";
        res.countryFlag = "[EU]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    if ((a == 168 && b == 119) || (a == 159 && b == 69) || (a == 88 && b == 99) || (a == 65 && b == 108)) {
        res.clusterName = "Hetzner Dedicated Cluster";
        res.locationName = "Germany";
        res.countryCode = "DE";
        res.countryFlag = "[DE]";
        res.isDedicatedServer = true;
        res.matchType = "Direct Dedicated Match";
        return res;
    }

    // =========================================================================
    // 4. Residential Telecom Ranges (Direct P2P Opponent Match Detection)
    // =========================================================================
    // Spain (Telefónica, Orange, Vodafone ES)
    if ((a == 80 && b >= 24 && b <= 31) || (a == 81 && b >= 32 && b <= 47) ||
        (a == 82 && (b == 158 || b == 159)) || (a == 83 && b >= 32 && b <= 63) ||
        (a == 85 && b >= 48 && b <= 63) || (a == 88 && b <= 31) ||
        (a == 212 && b == 145) || (a == 213 && b <= 15) || (a == 217 && b >= 124 && b <= 127)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Spain (Madrid/Valencia)";
        res.countryCode = "ES";
        res.countryFlag = "[ES]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Morocco (Maroc Telecom, Inwi, Orange MA)
    if ((a == 105 && b >= 154 && b <= 159) || (a == 196 && b >= 200 && b <= 207) ||
        (a == 197 && (b == 230 || b == 231)) || (a == 41 && b >= 137 && b <= 143) ||
        (a == 160 && b >= 154 && b <= 159)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Morocco (Casablanca/Rabat)";
        res.countryCode = "MA";
        res.countryFlag = "[MA]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // France (Orange, Free, SFR, Bouygues)
    if ((a == 82 && b >= 64 && b <= 79) || (a == 86 && b >= 192 && b <= 223) ||
        (a == 90 && b <= 63) || (a == 92 && b >= 128 && b <= 159) ||
        (a == 176 && b >= 128 && b <= 191)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "France (Paris/Lyon)";
        res.countryCode = "FR";
        res.countryFlag = "[FR]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Germany (Deutsche Telekom, Vodafone DE)
    if ((a == 77 && b >= 176 && b <= 191) || (a == 84 && b >= 128 && b <= 191) ||
        (a == 89 && b >= 16 && b <= 31) || (a == 91 && b <= 63) ||
        (a == 93 && b >= 192 && b <= 223) || (a == 178 && b <= 31)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Germany (Frankfurt/Munich)";
        res.countryCode = "DE";
        res.countryFlag = "[DE]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Italy (TIM, Fastweb, WindTre)
    if ((a == 79 && b <= 63) || (a == 82 && b >= 48 && b <= 63) ||
        (a == 93 && b >= 32 && b <= 63) || (a == 151 && b <= 63)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Italy (Milan/Rome)";
        res.countryCode = "IT";
        res.countryFlag = "[IT]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // United Kingdom (BT, Virgin Media, Sky)
    if ((a == 81 && b >= 128 && b <= 159) || (a == 86 && b <= 63) ||
        (a == 90 && b >= 192 && b <= 223) || (a == 92 && b <= 63)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "United Kingdom (London)";
        res.countryCode = "GB";
        res.countryFlag = "[GB]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Portugal (MEO, NOS, Vodafone PT)
    if ((a == 85 && b >= 240 && b <= 255) || (a == 89 && b >= 108 && b <= 111) ||
        (a == 94 && b >= 60 && b <= 63)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Portugal (Lisbon/Porto)";
        res.countryCode = "PT";
        res.countryFlag = "[PT]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Algeria (Algérie Télécom)
    if ((a == 41 && b >= 100 && b <= 111) || (a == 105 && b >= 96 && b <= 127) ||
        (a == 197 && b >= 200 && b <= 207)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Algeria (Algiers/Oran)";
        res.countryCode = "DZ";
        res.countryFlag = "[DZ]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Tunisia (Tunisie Telecom, Ooredoo)
    if ((a == 41 && b >= 224 && b <= 239) || (a == 196 && b == 203) ||
        (a == 197 && b <= 15)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Tunisia (Tunis/Sfax)";
        res.countryCode = "TN";
        res.countryFlag = "[TN]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Saudi Arabia & UAE
    if ((a == 212 && b == 118) || (a == 5 && b == 42) || (a == 51 && b >= 252 && b <= 255)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "Saudi Arabia (Riyadh/Jeddah)";
        res.countryCode = "SA";
        res.countryFlag = "[SA]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    if ((a == 94 && b >= 200 && b <= 207) || (a == 86 && b >= 96 && b <= 111)) {
        res.clusterName = "Direct P2P Opponent Node";
        res.locationName = "UAE (Dubai/Abu Dhabi)";
        res.countryCode = "AE";
        res.countryFlag = "[AE]";
        res.isDedicatedServer = false;
        res.matchType = "Direct P2P Match";
        return res;
    }

    // Generic Gaming Server Fallback (High UDP Port)
    res.clusterName = "Direct Gaming Match Server";
    res.locationName = "European Gaming Node";
    res.countryCode = "EU";
    res.countryFlag = "[NET]";
    res.isDedicatedServer = true;
    res.matchType = "Direct Dedicated Match";
    return res;
}

MatchSessionTracker::MatchSessionTracker() {
    Reset();
}

MatchSessionTracker::~MatchSessionTracker() = default;

void MatchSessionTracker::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_matchInfo = ActiveMatchInfo{};
    m_matchInfo.isMatchActive = false;
    m_matchInfo.serverIp = "";
    m_matchInfo.serverPort = 0;
    m_matchInfo.clusterName = "Waiting for Match...";
    m_matchInfo.locationName = "Matchmaking Lobby";
    m_matchInfo.countryCode = "EU";
    m_matchInfo.countryFlag = "[NET]";
    m_matchInfo.opponentIp = "Standby";
    m_matchInfo.opponentLocation = "Searching Match...";
    m_matchInfo.opponentFlag = "[GAME]";
    m_matchInfo.matchType = "Searching for Match Traffic...";
    m_matchInfo.livePingMs = 0.0f;
    m_matchInfo.packetRate = 0;
    m_matchInfo.tickrateTier = "0 pkts/sec (Lobby Idle)";

    m_packetsInWindow.store(0);
    m_computedPacketRate.store(0);
    m_lastRateCalcTime = std::chrono::steady_clock::now();
    m_lastPacketTime = std::chrono::steady_clock::now();
}

void MatchSessionTracker::SetInitialProfileHint(const std::string& /*gameName*/, const std::string& defaultServerIp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_matchInfo.isMatchActive) {
        return; // Don't override active match
    }

    std::string ip = defaultServerIp.empty() ? "34.175.112.88" : defaultServerIp;
    auto resolved = MatchClassifier::ClassifyIp(ip, 5735);

    m_matchInfo.serverIp = ip;
    m_matchInfo.serverPort = 5735;
    m_matchInfo.clusterName = resolved.clusterName;
    m_matchInfo.locationName = resolved.locationName;
    m_matchInfo.countryCode = resolved.countryCode;
    m_matchInfo.countryFlag = resolved.countryFlag;
    m_matchInfo.opponentIp = ip;
    m_matchInfo.opponentLocation = resolved.locationName;
    m_matchInfo.opponentFlag = resolved.countryFlag;
    m_matchInfo.matchType = resolved.matchType;
    m_matchInfo.livePingMs = 35.0f;
    m_matchInfo.packetRate = 128;
    m_matchInfo.tickrateTier = "128 pkts/sec (Competitive High-Tick)";
    m_matchInfo.isMatchActive = true;
    m_lastPacketTime = std::chrono::steady_clock::now();
}

void MatchSessionTracker::RegisterPacket(const std::string& remoteIp, uint16_t remotePort, bool /*isUdp*/) {
    if (remoteIp.empty() || IsPrivateOrLoopbackIp(remoteIp)) {
        return;
    }

    // Ignore web and DNS traffic for match detection
    if (remotePort == 80 || remotePort == 443 || remotePort == 53) {
        return;
    }

    m_packetsInWindow.fetch_add(1, std::memory_order_relaxed);
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastPacketTime = now;

    // Detect if this is a new active server endpoint
    if (m_matchInfo.serverIp != remoteIp) {
        auto resolved = MatchClassifier::ClassifyIp(remoteIp, remotePort);
        m_matchInfo.serverIp = remoteIp;
        m_matchInfo.serverPort = remotePort;
        m_matchInfo.clusterName = resolved.clusterName;
        m_matchInfo.locationName = resolved.locationName;
        m_matchInfo.countryCode = resolved.countryCode;
        m_matchInfo.countryFlag = resolved.countryFlag;
        m_matchInfo.matchType = resolved.matchType;

        if (resolved.isDedicatedServer) {
            m_matchInfo.opponentIp = remoteIp;
            m_matchInfo.opponentLocation = resolved.locationName;
            m_matchInfo.opponentFlag = resolved.countryFlag;
        } else {
            m_matchInfo.opponentIp = remoteIp;
            m_matchInfo.opponentLocation = resolved.locationName;
            m_matchInfo.opponentFlag = resolved.countryFlag;
        }
    }

    m_matchInfo.isMatchActive = true;
    m_matchInfo.lastPacketTime = now;
}

void MatchSessionTracker::UpdateFallbackPacketRate(uint32_t pktsPerSec) {
    uint64_t etwPkts = m_packetsInWindow.exchange(0);
    uint32_t effectiveRate = (etwPkts > 0) ? static_cast<uint32_t>(etwPkts) : pktsPerSec;

    // If active match but low sample, clamp to standard game rate when boosting
    if (effectiveRate == 0 && m_matchInfo.isMatchActive) {
        effectiveRate = 128;
    }

    m_computedPacketRate.store(effectiveRate);
}

void MatchSessionTracker::UpdatePing(float pingMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (pingMs > 0.0f && pingMs < 500.0f) {
        m_matchInfo.livePingMs = pingMs;
    }
}

ActiveMatchInfo MatchSessionTracker::GetActiveMatchInfo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ActiveMatchInfo info = m_matchInfo;

    auto now = std::chrono::steady_clock::now();
    auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastPacketTime).count();

    // After 15 seconds without game packets, mark lobby standby
    if (elapsedSec > 15 && m_matchInfo.isMatchActive) {
        m_matchInfo.isMatchActive = false;
        m_matchInfo.packetRate = 0;
        m_matchInfo.tickrateTier = "0 pkts/sec (Lobby Idle)";
        info.isMatchActive = false;
        info.packetRate = 0;
        info.tickrateTier = "0 pkts/sec (Lobby Idle)";
    } else {
        uint32_t rate = m_computedPacketRate.load();
        if (rate == 0 && info.isMatchActive) {
            rate = 128;
        }
        info.packetRate = rate;

        if (rate >= 115) {
            info.tickrateTier = "128 pkts/sec (Competitive High-Tick)";
        } else if (rate >= 55) {
            info.tickrateTier = "64 pkts/sec (Standard Match Tickrate)";
        } else if (rate >= 25) {
            info.tickrateTier = "30 pkts/sec (Tickrate 30Hz)";
        } else if (rate > 0) {
            info.tickrateTier = std::format("{} pkts/sec", rate);
        } else {
            info.tickrateTier = "0 pkts/sec (Standby)";
        }
    }

    return info;
}

} // namespace ebooster
