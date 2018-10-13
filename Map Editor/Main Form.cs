using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;

namespace Map_Editor
{
    public partial class form_MapEditor : Form
    {
        private BufferedGraphicsContext buf_con;
        private BufferedGraphics buf_grp;

        public form_MapEditor()
        {
            InitializeComponent();
            buf_con = BufferedGraphicsManager.Current;
            buf_con.MaximumBuffer = new Size(pn_MapViewer.Width + 1, pn_MapViewer.Height + 1);
            buf_grp = buf_con.Allocate(pn_MapViewer.CreateGraphics(), new Rectangle(0, 0, pn_MapViewer.Width, pn_MapViewer.Height));
        }

        private Int64 MapWidth = 0;
        private Int64 MapHeight = 0;
        private Int64[] MapArray;

        private void tb_MapWidth_KeyPress(object sender, KeyPressEventArgs e)
        {
            int tmp;
            e.Handled = !int.TryParse(e.KeyChar.ToString(), out tmp);
            if (char.IsControl(e.KeyChar))
            {
                e.Handled = false;
            }
        }

        private void tb_MapHeight_KeyPress(object sender, KeyPressEventArgs e)
        {
            int tmp;
            e.Handled = !int.TryParse(e.KeyChar.ToString(), out tmp);
            if (char.IsControl(e.KeyChar))
            {
                e.Handled = false;
            }
        }

        private void bt_New_Click(object sender, EventArgs e)
        {
            Int64.TryParse(tb_MapWidth.Text, out MapWidth);
            Int64.TryParse(tb_MapHeight.Text, out MapHeight);
            lb_MapSize.Text = $"Map Size = ({MapWidth}, {MapHeight})";
            MapArray = new Int64[MapWidth * MapHeight];
            buf_grp.Graphics.Clear(Color.Black);
            pn_MapViewer.Refresh();
        }

        private void pn_MapViewer_Paint(object sender, PaintEventArgs e)
        {
            if (!(MapWidth > 0 && MapHeight > 0))
            {
                return;
            }
            var r = new RectangleF();
            var o = (Panel)sender;

            //g.Clear(Color.Black);
            long Ruler = Math.Max(MapWidth, MapHeight);
            r.Width = o.Size.Width / Ruler - 1;
            r.Height = o.Size.Width / Ruler - 1;
            List<RectangleF> list_rect = new List<RectangleF>();
            
            for (int x = 0; x < MapWidth; ++x)
            {
                for (int y = 0; y < MapHeight; ++y)
                {
                    r.X = o.Size.Width * x / Ruler;
                    r.Y = o.Size.Height * y / Ruler;

                    if (MapArray[x + MapWidth * y] == 1)
                    {
                        list_rect.Add(r);
                    }
                }
            }
            if (list_rect.Count > 0)
            {
                buf_grp.Graphics.FillRectangles(Brushes.Green, list_rect.ToArray());
                list_rect.Clear();
            }
            for (int x = 0; x < MapWidth; ++x)
            {
                for (int y = 0; y < MapHeight; ++y)
                {
                    r.X = o.Size.Width * x / Ruler;
                    r.Y = o.Size.Height * y / Ruler;

                    if (MapArray[x + MapWidth * y] == 0)
                    {
                        list_rect.Add(r);
                    }
                }
            }

            if (list_rect.Count > 0)
            {
                buf_grp.Graphics.FillRectangles(Brushes.DarkGray, list_rect.ToArray());
                list_rect.Clear();
            }

            buf_grp.Render();

        }

        private void bt_SaveAs_Click(object sender, EventArgs e)
        {
            var svf = new SaveFileDialog();
            svf.FileName = "Untitled.map";
            svf.Filter = "Map files (*.map)|*.map";
            if (svf.ShowDialog() == DialogResult.OK)
            {
                this.Text = svf.FileName;

                List<string> buffer = new List<string>();

                buffer.Add($"{MapWidth} {MapHeight}");
                for (int y = 0; y < MapHeight; ++y)
                {
                    var sb = new StringBuilder($"{MapArray[MapWidth * y]}");
                    for (int x = 1; x < MapWidth; ++x)
                    {
                        sb.Append($" {MapArray[x + MapWidth * y]}");
                    }
                    buffer.Add(sb.ToString());
                }

                File.WriteAllLines(
                    svf.FileName,
                    buffer.ToArray()
                );
                bt_Save.Enabled = true;
            }
        }

        private void bt_Open_Click(object sender, EventArgs e)
        {
            var opf = new OpenFileDialog();
            opf.Filter = "Map files (*.map)|*.map";
            if (opf.ShowDialog() == DialogResult.OK)
            {
                this.Text = opf.FileName;
                List<string> list_str = new List<string>(File.ReadAllLines(opf.FileName));

                var header = list_str[0].Split(' ');
                MapWidth = int.Parse(header[0]);
                MapHeight = int.Parse(header[1]);
                MapArray = new Int64[MapWidth * MapHeight];

                for (int y = 0; y < MapHeight; ++y)
                {
                    var line_pix = list_str[1 + y].Split(' ');
                    for (int x = 0; x < MapWidth; ++x)
                    {
                        MapArray[x + MapWidth * y] = Int64.Parse(line_pix[x]);
                    }
                }

                buf_grp.Graphics.Clear(Color.Black);
                pn_MapViewer.Refresh();
                bt_Save.Enabled = true;
                //Debug.WriteLine();
            }
        }

        private void pn_MapViewer_Click(object sender, EventArgs e)
        {
            if (!(MapWidth > 0 && MapHeight > 0))
            {
                return;
            }

            var _e = (MouseEventArgs)e;

            MapArray[(_e.X * MapWidth / pn_MapViewer.Size.Width) + MapWidth * (_e.Y * MapHeight / pn_MapViewer.Size.Height)] = 1 - (MapArray[(_e.X * MapWidth / pn_MapViewer.Size.Width) + MapWidth * (_e.Y * MapHeight / pn_MapViewer.Size.Height)]);
            pn_MapViewer.Refresh();
            //Debug.WriteLine($"{(_e.X * MapWidth / pn_MapViewer.Size.Width)}, {(_e.Y * MapHeight / pn_MapViewer.Size.Height)} = {MapArray[(_e.X * MapWidth / pn_MapViewer.Size.Width) + MapWidth * (_e.Y * MapHeight / pn_MapViewer.Size.Height)]}");
        }

        private void bt_Save_Click(object sender, EventArgs e)
        {
            if (this.Text != "Map Editor")
            {
                List<string> buffer = new List<string>();

                buffer.Add($"{MapWidth} {MapHeight}");
                for (int y = 0; y < MapHeight; ++y)
                {
                    var sb = new StringBuilder($"{MapArray[MapWidth * y]}");
                    for (int x = 1; x < MapWidth; ++x)
                    {
                        sb.Append($" {MapArray[x + MapWidth * y]}");
                    }
                    buffer.Add(sb.ToString());
                }

                File.WriteAllLines(
                    this.Text,
                    buffer.ToArray()
                );
            }
            else
            {
                bt_Save.Enabled = false;
            }
        }
    }
}
