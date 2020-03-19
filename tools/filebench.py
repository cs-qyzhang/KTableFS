#!/usr/bin/ipython3 --pdb
import os;
import matplotlib;
import matplotlib.pyplot as plt;
import numpy as np;
# matplotlib.use("pgf")
# matplotlib.rcParams.update({
#     "pgf.texsystem": "xelatex",
#     'font.family': 'serif',
#     'text.usetex': True,
#     'pgf.rcfonts': False,
# })

filebench = "/home/qyzhang/src/filebench-1.5-alpha3/build/bin/filebench"

class Bench:
    def __init__(self, progs, benchfile, args):
        self.progs = progs;
        self.benchfile = benchfile;
        self.args = args;
        self.stats = [];

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
            self.run_cmd_("rm -r /home/qyzhang/src/tablefs-0.3/metadir/*");
            self.run_cmd_("rm -r /home/qyzhang/src/tablefs-0.3/datadir/*");
            cmd = prog + " -mountdir /home/qyzhang/Projects/GraduationProject/code/KTableFS/build/mount -metadir /home/qyzhang/src/tablefs-0.3/metadir -datadir /home/qyzhang/src/tablefs-0.3/datadir";
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

    def draw(self):
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
                data[i].append(op["ops/s"]);
            data[i].append(self.stats[i]["summary"]["ops/s"]);

        x = np.arange(len(labels));  # the label locations
        width = 0.9 / prog_nr;  # the width of the bars

        fig, ax = plt.subplots();
        rects = [];
        for i in range(prog_nr):
            rect = ax.bar(x - (prog_nr - 1) * width / 2.0 + i * width, data[i], width, label=self.stats[i]["name"]);
            rects.append(rect);

        # Add some text for labels, title and custom x-axis tick labels, etc.
        ax.set_ylabel('op/s');
        ax.set_title('op/s');
        ax.set_xticks(x);
        ax.set_xticklabels(labels);
        ax.legend();
        ax.set_axisbelow(True);
        ax.yaxis.grid(True, color='#EEEEEE');
        ax.xaxis.grid(False);

        for rect in rects:
            self.autolabel_(ax, rect);

        fig.tight_layout()

        plt.savefig("fig.pgf")
        plt.show()


if __name__ == "__main__":
    progs = [
        "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/ktablefs_ll",
        "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/ktablefs",
        "/home/qyzhang/src/tablefs-0.3/src/tablefs",
        "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/ext4-fuse"
    ];
    args = {};
    args["single thread"] = 1;
    args["datadir"] = "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/datadir";
    args["mountdir"] = "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/mount";
    bench = Bench(progs, "copyfiles.f", args);
    bench.run();
    bench.draw();