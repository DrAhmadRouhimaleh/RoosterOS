package profile

import (
    "fmt"
    "os"
    "regexp"
    "strings"

    "github.com/pelletier/go-toml/v2"
)

var (
    editions = map[string]bool{"low": true, "medium": true, "high": true, "server": true}
    arches   = map[string]bool{"x86": true, "x86_64": true, "arm": true, "arm64": true, "riscv": true}
    pkgRe    = regexp.MustCompile(`^[a-z0-9][a-z0-9_\-]*$`)
    resRe    = regexp.MustCompile(`^(\d+)x(\d+)$`)
)

// Graphics holds parsed gfx_min.
type Graphics struct {
    Vendor  string
    Width   int    // >0 if resolution parsed
    Height  int    // >0 if resolution parsed
    Feature string // driver/feature if not resolution
}

// UnmarshalTOML customizes parsing of gfx_min.
func (g *Graphics) UnmarshalTOML(v interface{}) error {
    s, ok := v.(string)
    if !ok {
        return fmt.Errorf("gfx_min must be string")
    }
    parts := strings.SplitN(s, " ", 2)
    if len(parts) != 2 {
        return fmt.Errorf("invalid gfx_min: %q", s)
    }
    g.Vendor = parts[0]
    spec := parts[1]

    if m := resRe.FindStringSubmatch(spec); m != nil {
        // resolution path
        fmt.Sscanf(m[1], "%d", &g.Width)
        fmt.Sscanf(m[2], "%d", &g.Height)
        if g.Width < 320 || g.Height < 200 {
            return fmt.Errorf("resolution %dx%d too small (min 320x200)", g.Width, g.Height)
        }
    } else {
        // feature path
        g.Feature = spec
    }
    return nil
}

// Profile defines the hardware config.
type Profile struct {
    Edition      string    `toml:"edition"`
    Arch         string    `toml:"arch"`
    RAMMinMB     int       `toml:"ram_min_mb"`
    RAMMaxMB     int       `toml:"ram_max_mb"`
    CPUMinMHz    int       `toml:"cpu_min_mhz"`
    CPUMaxMHz    int       `toml:"cpu_max_mhz,omitempty"` // optional
    GFXMin       Graphics  `toml:"gfx_min"`
    DiskMinGB    float64   `toml:"disk_min_gb"`
    Internet     bool      `toml:"internet"`
    Packages     []string  `toml:"packages"`
}

// Validate checks all fields and cross-field constraints.
func (p *Profile) Validate() error {
    if !editions[p.Edition] {
        return fmt.Errorf("edition %q not allowed", p.Edition)
    }
    if !arches[p.Arch] {
        return fmt.Errorf("arch %q not allowed", p.Arch)
    }
    if p.RAMMinMB < 16 || p.RAMMaxMB < p.RAMMinMB || p.RAMMaxMB > 65536 {
        return fmt.Errorf("invalid RAM range %d–%d (allowed 16–65536)", p.RAMMinMB, p.RAMMaxMB)
    }
    if p.CPUMinMHz < 100 {
        return fmt.Errorf("cpu_min_mhz %d too low (min 100)", p.CPUMinMHz)
    }
    if p.CPUMaxMHz == 0 {
        p.CPUMaxMHz = p.CPUMinMHz
    }
    if p.CPUMaxMHz < p.CPUMinMHz || p.CPUMaxMHz > 10000 {
        return fmt.Errorf("invalid CPU range %d–%d (allowed ≤10000)", p.CPUMinMHz, p.CPUMaxMHz)
    }
    if p.DiskMinGB < 1 {
        return fmt.Errorf("disk_min_gb %.1f too small (min 1)", p.DiskMinGB)
    }
    for _, pkg := range p.Packages {
        if !pkgRe.MatchString(pkg) {
            return fmt.Errorf("invalid package name %q", pkg)
        }
    }
    return nil
}

// Load reads, parses, and validates a TOML profile.
func Load(path string) (*Profile, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, err
    }
    var p Profile
    if err := toml.Unmarshal(data, &p); err != nil {
        return nil, err
    }
    if err := p.Validate(); err != nil {
        return nil, err
    }
    return &p, nil
}
