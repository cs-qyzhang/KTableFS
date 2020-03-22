#!/usr/bin/ipython3 --pdb
import os;
import matplotlib;
import matplotlib.pyplot as plt;
import numpy as np;
from datetime import datetime;

path_prefix = "/home/qyzhang/Projects/GraduationProject/code/"
# path_prefix = "/root/"

machine_name = "ThinkPad-T490"
filebench = path_prefix + "filebench-1.5-alpha3/build/bin/filebench"
perf_name_dict = {"ops/s": "IOPS", "mb/s": "bandwidth", "ms/op": "latency", "max ms/op": "max latency", "min ms/op": "min latency"};

class Bench:
    def __init__(self, progs, benchfile, args, machine):
        self.progs = progs;
        self.benchfile = benchfile;
        self.args = args;
        self.stats = [];
        self.machine = machine;
        if os.path.exists("Manifest"):
            with open("Manifest", 'r+') as f:
                s = f.read();
                self.seq = int(s) + 1;
                f.seek(0);
                f.write(str(self.seq));
        else:
            with open("Manifest", 'w+') as f:
                self.seq = 1;
                f.write(str(0));

    def parse_args(self):
        arg_list = [];
        if self.args:
            if "single thread" in self.args:
                arg_list.append("-s");
            if "foreground" in self.args:
                arg_list.append("-f");
            arg_list.append("--datadir="+self.args["datadir"]);
            arg_list.append(self.args["mountdir"]);
        return " ".join(arg_list);

    def run_cmd_(self, cmd):
        print(cmd);
        return os.system(cmd);

    def mount_(self, prog):
        cmd = "";
        if prog.split('/')[-1] == "tablefs":
            self.run_cmd_("rm -r " + path_prefix + "KTableFS/build/metadir/*");
            self.run_cmd_("rm -r " + path_prefix + "KTableFS/build/datadir/*");
            cmd = prog + " -mountdir " + path_prefix + "KTableFS/build/mount -metadir " + path_prefix + "KTableFS/build/metadir -datadir " + path_prefix + "KTableFS/build/datadir";
        else:
            cmd = prog + " " + self.parse_args();
        print("mount", prog);
        print("    ", cmd, end=" ");
        if os.system(cmd):
            print("-> ERROR");
            return False;
        else:
            print("-> Success");
            return True;

    def prepare_(self):
        self.run_cmd_("fusermount -u " + self.args["mountdir"]);
        self.run_cmd_("rm -r " + self.args["datadir"] + "/*");
        self.run_cmd_("rm -r " + self.args["mountdir"] + "/*");

    def statistic_(self, prog, output:str):
        print(output);
        lines = output.splitlines();
        i = 0;
        begin_time = 0.0;
        stat = {};
        stat["name"] = prog.split('/')[-1];
        while True:
            words = lines[i].split();
            if words[1] == "Running...":
                begin_time = float(words[0][:-1]);
                i += 1;
                break;
            i += 1;
        words = lines[i].split();
        end_time = float(words[0][:-1]);
        print("run time: {0:.3f}".format(end_time - begin_time));
        stat["run time"] = end_time - begin_time;
        stat["per-operation"] = [];
        i += 2;
        while True:
            words = lines[i].split();
            if (words[0][-1] == ':'):
                break;
            else:
                op = {};
                op["name"] = words[0];
                op["ops"] = int(words[1][:-3]);
                op["ops/s"] = int(words[2][:-5]);
                op["mb/s"] = float(words[3][:-4]);
                op["ms/op"] = float(words[4][:-5]);
                op["min ms/op"] = float(words[5][1:-2]);
                op["max ms/op"] = float(words[7][:-3]);
                stat["per-operation"].append(op);
            i += 1;
        words = lines[i].split();
        stat["summary"] = {};
        stat["summary"]["ops"] = int(words[3]);
        stat["summary"]["ops/s"] = float(words[5]);
        stat["summary"]["rd"] = int(words[7].split('/')[0]);
        stat["summary"]["wr"] = int(words[7].split('/')[1]);
        stat["summary"]["mb/s"] = float(words[9][:-4]);
        stat["summary"]["ms/op"] = float(words[10][:-5]);
        self.stats.append(stat);

    def run(self):
        self.prepare_();
        for prog in self.progs:
            self.mount_(prog);
            bench_cmd = filebench + " -f " + self.benchfile + " 2>/dev/null";
            print("benchmark", bench_cmd);
            stream = os.popen(bench_cmd);
            output = stream.read();
            self.prepare_();
            self.statistic_(prog, output);

    def autolabel_(self, ax, rects):
        """Attach a text label above each bar in *rects*, displaying its height."""
        for rect in rects:
            height = rect.get_height();
            ax.annotate('{}'.format(height),
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom');

    def draw_figure(self, perf, show=False):
        prog_nr = len(self.stats);
        if prog_nr == 0:
            return;

        labels = [];
        for op in self.stats[0]["per-operation"]:
            labels.append(op["name"]);
        labels.append("total");

        data = [[] for i in range(prog_nr)];
        for i in range(prog_nr):
            for op in self.stats[i]["per-operation"]:
                data[i].append(op[perf]);
            data[i].append(self.stats[i]["summary"][perf]);

        x = np.arange(len(labels));  # the label locations
        width = 0.9 / prog_nr;  # the width of the bars

        fig, ax = plt.subplots();
        for i in range(prog_nr):
            rect = ax.bar(x - (prog_nr - 1) * width / 2.0 + i * width, data[i], width, label=self.stats[i]["name"]);
            # self.autolabel_(ax, rect);

        ax.set_ylabel(perf);
        ax.set_title(perf_name_dict[perf]);
        ax.set_xticks(x);
        ax.set_xticklabels(labels);
        ax.legend();
        ax.set_axisbelow(True);
        ax.yaxis.grid(True, color='#EEEEEE');
        ax.xaxis.grid(False);

        fig.tight_layout();

        fig_name = perf_name_dict[perf] + "-" + str(self.seq);
        plt.savefig(fig_name + ".pgf");
        plt.savefig(fig_name + ".pdf");
        if show:
            plt.show();

        tex = r"""
\begin{figure}[!htbp]
\begin{center}
   \scalebox{0.9}{\input{""" + fig_name + r""".pgf}}
\end{center}
\end{figure}
""";
        return tex;

    def draw_table(self):
        prog_nr = len(self.stats);
        if prog_nr == 0:
            return;

        tex = r"""
\begin{table}[!htbp]
\centering
\setlength{\tabcolsep}{12pt}
\setlength\doublerulesep{1.5pt}
\catcode`_=12
\begin{tabular}{""" + "|c" * (2 + prog_nr)  + r"""|}
\hline
\addstackgap[8pt]{\textbf{\large 操作}} & \textbf{\large 指标}""";

        for i in range(prog_nr):
            tex = tex + r" & \textbf{\large " + self.stats[i]["name"] + "}";
        tex += r" \\\hline" + "\n";

        labels = [];
        for op in self.stats[0]["per-operation"]:
            labels.append(op["name"]);

        for i in range(len(labels)):
            tex += r"\multirow{5}{*}{" + labels[i] + "}\n";
            for perf in ["ops/s", "mb/s", "ms/op", "min ms/op", "max ms/op"]:
                tex += " & " + perf_name_dict[perf] + " (" + perf.split()[-1] + ")";
                data = [];
                for j in range(prog_nr):
                    data.append(self.stats[j]["per-operation"][i][perf]);
                min_ = min(data);
                max_ = max(data);
                if perf[-2:] == "op":
                    min_, max_ = max_, min_;
                for d in data:
                    tex += " & ";
                    if min_ != max_ and d == min_:
                        tex += r"\cellcolor{mincolor}";
                    elif min_ != max_ and d == max_:
                        tex += r"\cellcolor{maxcolor}";
                    tex += str(d);
                if perf != "max ms/op":
                    tex += r"\\\cline{2-" + str(prog_nr + 2) + "}\n";
                else:
                    tex += r"\\\hhline{|" + "=" * (prog_nr + 2) + "|}\n";

        tex += r"\multirow{4}{*}{total}";
        for perf in ["ops/s", "mb/s", "ms/op"]:
            tex += " & " + perf_name_dict[perf] + " (" + perf.split()[-1] + ")";
            data = [];
            for j in range(prog_nr):
                data.append(self.stats[j]["summary"][perf]);
            min_ = min(data);
            max_ = max(data);
            if perf[-2:] == "op":
                min_, max_ = max_, min_;
            for d in data:
                tex += " & ";
                if min_ != max_ and d == min_:
                    tex += r"\cellcolor{mincolor}";
                elif min_ != max_ and d == max_:
                    tex += r"\cellcolor{maxcolor}";
                tex += str(d);
            tex += r"\\\cline{2-" + str(prog_nr + 2) + "}\n";

        tex += " & run time (s)";
        data = [];
        for j in range(prog_nr):
            data.append(self.stats[j]["run time"]);
        min_ = "{:.3f}".format(max(data));
        max_ = "{:.3f}".format(min(data));
        for d in data:
            tex += " & ";
            d = "{:.3f}".format(d);
            if min_ != max_ and d == min_:
                tex += r"\cellcolor{mincolor}";
            elif min_ != max_ and d == max_:
                tex += r"\cellcolor{maxcolor}";
            tex += d;
        tex += r"""\\\hline
\end{tabular}
\end{table}
""";
        return tex;

    def summary(self, latexmk=False):
        file_name = "summary-" + str(self.seq);
        with open(file_name + ".tex", "w+") as f:
            preamble = r"""\documentclass{ctexart}
\usepackage{graphicx}
\usepackage{tikz}
\usepackage{colortbl}
\usepackage{xcolor}
\usepackage{multirow}
\usepackage{hhline}
\usepackage{stackengine}
\usepackage[left=2cm,right=2cm,top=2cm,bottom=2cm]{geometry}

\setCJKmainfont[%
  BoldFont=Source Han Serif CN Bold,%
  ItalicFont=FZKai-Z03S,%
  BoldItalicFont=FZCuKaiS-R-GB]{Source Han Serif CN}
\setCJKsansfont[%
  BoldFont=Source Han Sans CN Bold,%
  ItalicFont=FZKai-Z03S,%
  BoldItalicFont=FZCuKaiS-R-GB]{Source Han Sans CN}
\setCJKmonofont[%
  BoldFont=Source Han Sans CN Bold,%
  ItalicFont=FZKai-Z03S,%
  BoldItalicFont=FZCuKaiS-R-GB]{Source Han Sans CN}
\newCJKfontfamily\kai[BoldFont=FZCuKaiS-R-GB]{FZKai-Z03S}
\newCJKfontfamily\xiaobiaosong[BoldFont=FZXiaoBiaoSong-B05S]{FZXiaoBiaoSong-B05S}
\newCJKfontfamily\dabiaosong[BoldFont=FZDaBiaoSong-B06S]{FZDaBiaoSong-B06S}
\setmainfont{TeX Gyre Termes}
\setsansfont{Gotham Rounded Medium}

\definecolor{maxcolor}{rgb}{0.24, 0.71, 0.54}
\definecolor{mincolor}{rgb}{0.89, 0.26, 0.2}

\pagestyle{plain}

\begin{document}

\newgeometry{left=3cm,right=3cm,top=3cm,bottom=3cm}
""";
            f.write(preamble);

            f.write(r"""
\begin{center}
\Huge\dabiaosong\noindent KTableFS测试报告
\end{center}
\vspace{3em}

{\Large\xiaobiaosong\noindent 测试环境}

\begin{description}
""");
            def write_item(name, content):
                f.write(r"\item[" + name + ":] " + content + "\n");

            write_item("Machine Type", self.machine);

            stream = os.popen("lscpu | grep 'Model name'");
            cpu = stream.read()[:-1].split(':')[-1].strip();
            stream = os.popen("lscpu | grep 'CPU(s):' | head -1");
            core = stream.read()[:-1].split(':')[-1].strip();
            write_item("CPU Model", cpu + ", " + core + " Core");

            stream = os.popen("lsmem | grep 'online memory'");
            write_item("RAM", stream.read()[:-1].split(':')[1].strip());

            stream = os.popen("df -P " + self.args["mountdir"] + " | awk 'END{print $1}'");
            partition = stream.read()[:-1];
            stream = os.popen("cat /sys/class/block/" + partition.split('/')[-1] + "/dev");
            dev = stream.read()[:-1];
            dev = dev.split(':')[0] + ":0";
            disk_model = "Unknown Model"
            if os.path.exists("/sys/dev/block/" + dev + "/device/model"):
                stream = os.popen("cat /sys/dev/block/" + dev + "/device/model");
                disk_model = stream.read()[:-1].strip();
            stream = os.popen("cat /sys/dev/block/" + dev + "/size");
            disk_size = int(int(stream.read()[:-1]) * 512.0 / 1024 / 1024 / 1024);
            write_item("Disk Model", disk_model + " (" + str(disk_size) + "G)");

            stream = os.popen("hostnamectl | grep 'Operating System'");
            os_release = stream.read()[:-1].split(':')[-1].strip();
            write_item("OS", os_release)

            stream = os.popen("hostnamectl | grep 'Kernel'");
            kernel = stream.read()[:-1].split(':')[-1].strip();
            write_item("Kernel", kernel);

            write_item("Benchmark Workload", self.benchfile.split('.')[0]);

            with open(self.benchfile, 'r') as bench:
                for line in bench.readlines():
                    if len(line) > 3 and line[:3] == "set":
                        param = line.split()[-1].split('=')[0][1:];
                        if param == "meanfilesize":
                            file_size = line.split()[-1].split('=')[1];
                            if file_size[-1] == 'k':
                                write_item("Benchmark File Size", file_size[:-1] + " KB");
                            if file_size[-1] == 'm':
                                write_item("Benchmark File Size", file_size[:-1] + " MB");
                            if file_size[-1] == 'g':
                                write_item("Benchmark File Size", file_size[:-1] + " GB");
                        elif param == "nfiles":
                            nfiles = int(line.split()[-1].split('=')[1]);
                            magtitude = 0;
                            while nfiles >= 1000:
                                nfiles = nfiles // 1000;
                                magtitude += 1;
                            write_item("Benchmark Number of File", str(nfiles) + ", 000" * magtitude);

            with open("../CMakeLists.txt", 'r') as cmake:
                for line in cmake.readlines():
                    if len(line) > 5 and line[:3]  == "set":
                        param = line.split()[0].split('(')[-1];
                        if param == "PAGECACHE_NR_PAGE":
                            pagecache_size = 4096 * int(line.split()[1][:-1]);
                            if pagecache_size > 1024 * 1024 * 1024:
                                write_item("KTableFS Pagecache Size", str(int(pagecache_size / 1024 / 1024 / 1024)) + " GB");
                            elif pagecache_size > 1024 * 1024:
                                write_item("KTableFS Pagecache Size", str(int(pagecache_size / 1024 / 1024)) + " MB");
                            elif pagecache_size > 1024:
                                write_item("KTableFS Pagecache Size", str(int(pagecache_size / 1024)) + " KB");
                            else:
                                write_item("KTableFS Pagecache Size", str(int(pagecache_size)) + " Byte");
                        elif param == "INDEX_TYPE":
                            index = line.split()[1][:-1];
                            if index == 'btree':
                                write_item("KTableFS Index Type", "B Tree");
                            elif index == 'rbtree':
                                write_item("KTableFS Index Type", "Red-Black Tree");
                        elif param == "AGGREGATION_SLAB_SIZE":
                            write_item("KTableFS Aggregation File Slab Size", line.split()[1][:-1] + " KB");
                        elif param == "KVENGINE_THREAD_NR":
                            write_item("KTableFS Thread Number", line.split()[1][:-1]);

            if self.args["single thread"]:
                write_item("FUSE Multi Thread", "No");
            else:
                write_item("FUSE Multi Thread", "Yes");

            f.write(r"""\end{description}

\vfill
\begin{flushright}
张丘洋\\
测试时间：""" + datetime.now().strftime("%Y 年 %-m 月 %-d 日 %H:%M") + r"""\\
Generated by \LaTeX, Python and Matplotlib
\end{flushright}
\clearpage
\restoregeometry
""");

            f.write(self.draw_figure("ops/s"));
            f.write(self.draw_figure("ms/op"));
            f.write(self.draw_table());

            f.write(r"\end{document}" + "\n");
            f.close();
            self.run_cmd_("rm summary.tex");
            self.run_cmd_("ln -s " + file_name + ".tex summary.tex");

        if (latexmk):
            self.run_cmd_("latexmk " + file_name + ".tex > /dev/null");
            self.run_cmd_("latexmk -c " + file_name + ".tex > /dev/null");
            self.run_cmd_("rm summary.pdf");
            self.run_cmd_("ln -s " + file_name + ".pdf summary.pdf");

if __name__ == "__main__":
    progs = [
        path_prefix + "KTableFS/script/tablefs",
        path_prefix + "KTableFS/script/ktablefs",
        path_prefix + "KTableFS/build/ext4-fuse",
        path_prefix + "KTableFS/build/ktablefs_ll",
    ];
    args = {};
    args["single thread"] = 1;
    args["datadir"] = path_prefix + "KTableFS/build/datadir";
    args["mountdir"] = path_prefix + "KTableFS/build/mount";
    bench = Bench(progs, "copyfiles.f", args, machine_name);
    bench.run();
    bench.summary(True);