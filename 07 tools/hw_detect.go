// system_info.go
//
// A comprehensive, concurrent system information tool for Linux.
// Gathers OS release, kernel, CPU, memory, disk, network, uptime, and load.
// Outputs nicely formatted tables.
//
// Usage:
//   go run system_info.go

package main

import (
    "bufio"
    "fmt"
    "net"
    "os"
    "os/exec"
    "path/filepath"
    "runtime"
    "strconv"
    "strings"
    "sync"
    "syscall"
    "text/tabwriter"
    "time"
)

// OSInfo holds distribution and kernel info.
type OSInfo struct {
    Name        string
    Version     string
    Kernel      string
}

// CPUInfo holds processor details.
type CPUInfo struct {
    VendorID    string
    ModelName   string
    Family      string
    Model       string
    Stepping    string
    Cores       int // physical
    Threads     int // logical
    Flags       []string
    MHz         float64
}

// MemInfo holds memory statistics in bytes.
type MemInfo struct {
    Total       uint64
    Free        uint64
    Available   uint64
    Buffers     uint64
    Cached      uint64
    SwapTotal   uint64
    SwapFree    uint64
}

// DiskInfo holds filesystem usage.
type DiskInfo struct {
    MountPoint  string
    FsType      string
    Total       uint64
    Free        uint64
    Available   uint64
}

// NetInfo holds network interface details.
type NetInfo struct {
    Name        string
    HardwareAddr string
    Addrs       []string
}

// UptimeLoad holds uptime and load averages.
type UptimeLoad struct {
    Uptime      time.Duration
    Load1       float64
    Load5       float64
    Load15      float64
}

func main() {
    var (
        osInfo   OSInfo
        cpuInfo  CPUInfo
        memInfo  MemInfo
        disks    []DiskInfo
        nets     []NetInfo
        ul       UptimeLoad
        wg       sync.WaitGroup
    )

    wg.Add(6)
    go func() { defer wg.Done(); osInfo = detectOS() }()
    go func() { defer wg.Done(); cpuInfo = detectCPU() }()
    go func() { defer wg.Done(); memInfo = detectMem() }()
    go func() { defer wg.Done(); disks = detectDisks() }()
    go func() { defer wg.Done(); nets  = detectNet() }()
    go func() { defer wg.Done(); ul    = detectUptimeLoad() }()
    wg.Wait()

    // Print results
    tw := tabwriter.NewWriter(os.Stdout, 0, 8, 2, ' ', 0)

    fmt.Fprintln(tw, "== OS ==")
    fmt.Fprintf(tw, "Distro:\t%s %s\n", osInfo.Name, osInfo.Version)
    fmt.Fprintf(tw, "Kernel:\t%s\n\n", osInfo.Kernel)

    fmt.Fprintln(tw, "== CPU ==")
    fmt.Fprintf(tw, "Vendor:\t%s\n", cpuInfo.VendorID)
    fmt.Fprintf(tw, "Model:\t%s\n", cpuInfo.ModelName)
    fmt.Fprintf(tw, "Family:\t%s Model:\t%s Stepping:\t%s\n", cpuInfo.Family, cpuInfo.Model, cpuInfo.Stepping)
    fmt.Fprintf(tw, "Cores (phys/logical):\t%d/%d\n", cpuInfo.Cores, cpuInfo.Threads)
    fmt.Fprintf(tw, "Frequency (MHz):\t%.2f\n", cpuInfo.MHz)
    fmt.Fprintf(tw, "Flags:\t%s\n\n", strings.Join(cpuInfo.Flags, " "))

    fmt.Fprintln(tw, "== Memory (MB) ==")
    fmt.Fprintf(tw, "Total:\t%d\tFree:\t%d\tAvailable:\t%d\n",
        memInfo.Total/1024/1024, memInfo.Free/1024/1024, memInfo.Available/1024/1024)
    fmt.Fprintf(tw, "Buffers:\t%d\tCached:\t%d\n",
        memInfo.Buffers/1024/1024, memInfo.Cached/1024/1024)
    fmt.Fprintf(tw, "Swap Total:\t%d\tSwap Free:\t%d\n\n",
        memInfo.SwapTotal/1024/1024, memInfo.SwapFree/1024/1024)

    fmt.Fprintln(tw, "== Disks ==")
    fmt.Fprintf(tw, "Mount\tType\tTotal(GB)\tFree(GB)\tAvail(GB)\n")
    for _, d := range disks {
        fmt.Fprintf(tw, "%s\t%s\t%.2f\t%.2f\t%.2f\n",
            d.MountPoint, d.FsType,
            float64(d.Total)/1e9, float64(d.Free)/1e9, float64(d.Available)/1e9)
    }
    fmt.Fprintln(tw)

    fmt.Fprintln(tw, "== Network Interfaces ==")
    fmt.Fprintf(tw, "Name\tHWAddr\tAddresses\n")
    for _, ni := range nets {
        fmt.Fprintf(tw, "%s\t%s\t%s\n",
            ni.Name, ni.HardwareAddr, strings.Join(ni.Addrs, ","))
    }
    fmt.Fprintln(tw)

    fmt.Fprintln(tw, "== Uptime & Load ==")
    fmt.Fprintf(tw, "Uptime:\t%s\n", ul.Uptime.Truncate(time.Second))
    fmt.Fprintf(tw, "Load Avg (1/5/15):\t%.2f\t%.2f\t%.2f\n",
        ul.Load1, ul.Load5, ul.Load15)

    tw.Flush()
}

// detectOS reads /etc/os-release and `uname -r`.
func detectOS() OSInfo {
    info := OSInfo{"unknown", "unknown", "unknown"}
    if fr, err := os.Open("/etc/os-release"); err == nil {
        defer fr.Close()
        scanner := bufio.NewScanner(fr)
        for scanner.Scan() {
            line := scanner.Text()
            if strings.HasPrefix(line, "PRETTY_NAME=") {
                parts := strings.SplitN(line, "=", 2)
                val := strings.Trim(parts[1], `"'`)
                info.Name = val
            }
            if strings.HasPrefix(line, "VERSION_ID=") {
                parts := strings.SplitN(line, "=", 2)
                info.Version = strings.Trim(parts[1], `"'`)
            }
        }
    }
    if k, err := exec.Command("uname", "-r").Output(); err == nil {
        info.Kernel = strings.TrimSpace(string(k))
    }
    return info
}

// detectCPU parses /proc/cpuinfo for CPU details.
func detectCPU() CPUInfo {
    info := CPUInfo{}
    info.Threads = runtime.NumCPU()
    file, err := os.Open("/proc/cpuinfo")
    if err != nil {
        return info
    }
    defer file.Close()

    physCores := map[string]bool{}
    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
        line := scanner.Text()
        if !strings.Contains(line, ":") {
            continue
        }
        parts := strings.SplitN(line, ":", 2)
        key := strings.TrimSpace(parts[0])
        val := strings.TrimSpace(parts[1])

        switch key {
        case "vendor_id":
            info.VendorID = val
        case "model name":
            info.ModelName = val
        case "cpu family":
            info.Family = val
        case "model":
            info.Model = val
        case "stepping":
            info.Stepping = val
        case "cpu MHz":
            if f, err := strconv.ParseFloat(val, 64); err == nil {
                info.MHz = f
            }
        case "flags":
            info.Flags = strings.Fields(val)
        case "core id":
            // combine with physical id to count unique cores
            phys := "" 
            if parts := strings.Split(val, " "); len(parts) > 0 {
                phys = parts[0]
            }
            physCores[phys] = true
        }
    }
    if len(physCores) > 0 {
        info.Cores = len(physCores)
    }
    return info
}

// detectMem parses /proc/meminfo for memory stats.
func detectMem() MemInfo {
    m := MemInfo{}
    file, err := os.Open("/proc/meminfo")
    if err != nil {
        return m
    }
    defer file.Close()

    parseKB := func(s string) uint64 {
        v, _ := strconv.ParseUint(s, 10, 64)
        return v * 1024
    }

    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
        f := strings.Fields(scanner.Text())
        if len(f) < 2 {
            continue
        }
        key := strings.TrimSuffix(f[0], ":")
        val := parseKB(f[1])
        switch key {
        case "MemTotal":
            m.Total = val
        case "MemFree":
            m.Free = val
        case "MemAvailable":
            m.Available = val
        case "Buffers":
            m.Buffers = val
        case "Cached":
            m.Cached = val
        case "SwapTotal":
            m.SwapTotal = val
        case "SwapFree":
            m.SwapFree = val
        }
    }
    return m
}

// detectDisks inspects mounted filesystems via /proc/mounts.
func detectDisks() []DiskInfo {
    var res []DiskInfo
    file, err := os.Open("/proc/mounts")
    if err != nil {
        return res
    }
    defer file.Close()

    scanner := bufio.NewScanner(file)
    seen := map[string]bool{}
    for scanner.Scan() {
        f := strings.Fields(scanner.Text())
        if len(f) < 3 {
            continue
        }
        mount, fs := f[1], f[2]
        if seen[mount] || fs == "tmpfs" || fs == "proc" || fs == "sysfs" {
            continue
        }
        seen[mount] = true
        var st syscall.Statfs_t
        if err := syscall.Statfs(mount, &st); err != nil {
            continue
        }
        res = append(res, DiskInfo{
            MountPoint: mount,
            FsType:     fs,
            Total:      st.Blocks * uint64(st.Bsize),
            Free:       st.Bfree * uint64(st.Bsize),
            Available:  st.Bavail * uint64(st.Bsize),
        })
    }
    return res
}

// detectNet enumerates network interfaces and addresses.
func detectNet() []NetInfo {
    var out []NetInfo
    ifs, err := net.Interfaces()
    if err != nil {
        return out
    }
    for _, iface := range ifs {
        if (iface.Flags & net.FlagUp) == 0 {
            continue
        }
        var addrs []string
        if al, err := iface.Addrs(); err == nil {
            for _, a := range al {
                addrs = append(addrs, a.String())
            }
        }
        out = append(out, NetInfo{
            Name:        iface.Name,
            HardwareAddr: iface.HardwareAddr.String(),
            Addrs:       addrs,
        })
    }
    return out
}

// detectUptimeLoad reads /proc/uptime and /proc/loadavg.
func detectUptimeLoad() UptimeLoad {
    ul := UptimeLoad{}
    // uptime
    if data, err := os.ReadFile("/proc/uptime"); err == nil {
        fields := strings.Fields(string(data))
        if secs, err := strconv.ParseFloat(fields[0], 64); err == nil {
            ul.Uptime = time.Duration(secs) * time.Second
        }
    }
    // loadavg
    if data, err := os.ReadFile("/proc/loadavg"); err == nil {
        fields := strings.Fields(string(data))
        if f1, err := strconv.ParseFloat(fields[0], 64); err == nil {
            ul.Load1 = f1
        }
        if f5, err := strconv.ParseFloat(fields[1], 64); err == nil {
            ul.Load5 = f5
        }
        if f15, err := strconv.ParseFloat(fields[2], 64); err == nil {
            ul.Load15 = f15
        }
    }
    return ul
}
