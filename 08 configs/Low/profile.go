// profile.go
package profile

import (
    "fmt"
    "regexp"

    "github.com/pelletier/go-toml/v2"
)

var (
    editionAllowed = map[string]bool{
        "low": true, "medium": true, "high": true, "server": true,
    }
    archAllowed = map[string]bool{
        "x86": true, "x86_64": true, "arm": true, "arm64": true, "riscv": true,
    }
    pkgRe = regexp.MustCompile(`^[a-z0-9\-]+$`)
)

type Graphics struct {
    Vendor string
    Width  int
    Height int
}

type Profile struct {
    Edition    string   `toml:"edition"`
    Arch       string   `toml:"arch"`
    RAMMinMB   int      `toml:"ram_min_mb"`
    RAMMaxMB   int      `toml:"ram_max_mb"`
    CPUMinMHz  int      `toml:"cpu_min_mhz"`
    CPUMaxMHz  int      `toml:"cpu_max_mhz"`
    GFXMin     Graphics `toml:"gfx_min"`
    DiskMinGB  float64  `toml:"disk_min_gb"`
    Internet   bool     `toml:"internet"`
    Packages   []string `toml:"packages"`
}

// UnmarshalTOML custom parser for gfx_min
func (g *Graphics) UnmarshalTOML(v any) error {
    s, ok := v.(string)
    if !ok {
        return fmt.Errorf("GFXMin must be string, got %T", v)
    }
    parts := strings.Fields(s)
    if len(parts) != 2 {
        return fmt.Errorf("invalid gfx_min: '%s'", s)
    }
    dims := strings.SplitN(parts[1], "x", 2)
    w, err := strconv.Atoi(dims[0])
    if err != nil {
        return err
    }
    h, err := strconv.Atoi(dims[1])
    if err != nil {
        return err
    }
    if w < 320 || h < 200 {
        return fmt.Errorf("gfx_min must be ≥ 320x200, got %dx%d", w, h)
    }
    g.Vendor = parts[0]
    g.Width = w
    g.Height = h
    return nil
}

// Validate cross-field logic
func (p *Profile) Validate() error {
    if !editionAllowed[p.Edition] {
        return fmt.Errorf("edition: %s not allowed", p.Edition)
    }
    if !archAllowed[p.Arch] {
        return fmt.Errorf("arch: %s not allowed", p.Arch)
    }
    if p.RAMMinMB < 16 || p.RAMMaxMB > 32768 || p.RAMMinMB > p.RAMMaxMB {
        return fmt.Errorf("ram range invalid: %d–%d", p.RAMMinMB, p.RAMMaxMB)
    }
    if p.CPUMinMHz < 50 || p.CPUMaxMHz > 5000 || p.CPUMinMHz > p.CPUMaxMHz {
        return fmt.Errorf("cpu range invalid: %d–%d", p.CPUMinMHz, p.CPUMaxMHz)
    }
    if p.DiskMinGB < 1 {
        return fmt.Errorf("disk_min_gb must be ≥1, got %f", p.DiskMinGB)
    }
    for _, pkg := range p.Packages {
        if !pkgRe.MatchString(pkg) {
            return fmt.Errorf("invalid package name: %s", pkg)
        }
    }
    return nil
}

// LoadProfile reads and validates a TOML profile.
func LoadProfile(path string) (*Profile, error) {
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
