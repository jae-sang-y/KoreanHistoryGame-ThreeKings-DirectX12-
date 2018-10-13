namespace Map_Editor
{
    partial class form_MapEditor
    {
        /// <summary>
        /// 필수 디자이너 변수입니다.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// 사용 중인 모든 리소스를 정리합니다.
        /// </summary>
        /// <param name="disposing">관리되는 리소스를 삭제해야 하면 true이고, 그렇지 않으면 false입니다.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form 디자이너에서 생성한 코드

        /// <summary>
        /// 디자이너 지원에 필요한 메서드입니다. 
        /// 이 메서드의 내용을 코드 편집기로 수정하지 마세요.
        /// </summary>
        private void InitializeComponent()
        {
            this.bt_New = new System.Windows.Forms.Button();
            this.tb_MapWidth = new System.Windows.Forms.TextBox();
            this.tb_MapHeight = new System.Windows.Forms.TextBox();
            this.pn_MapViewer = new System.Windows.Forms.Panel();
            this.pn_ToolSelect = new System.Windows.Forms.Panel();
            this.lb_MapSize = new System.Windows.Forms.Label();
            this.bt_SaveAs = new System.Windows.Forms.Button();
            this.bt_Open = new System.Windows.Forms.Button();
            this.rb_Pen = new System.Windows.Forms.RadioButton();
            this.rb_Line = new System.Windows.Forms.RadioButton();
            this.rb_Rectangle = new System.Windows.Forms.RadioButton();
            this.rb_None = new System.Windows.Forms.RadioButton();
            this.bt_Save = new System.Windows.Forms.Button();
            this.pn_MapViewer.SuspendLayout();
            this.SuspendLayout();
            // 
            // bt_New
            // 
            this.bt_New.Location = new System.Drawing.Point(804, 51);
            this.bt_New.Name = "bt_New";
            this.bt_New.Size = new System.Drawing.Size(50, 23);
            this.bt_New.TabIndex = 0;
            this.bt_New.Text = "New";
            this.bt_New.UseVisualStyleBackColor = true;
            this.bt_New.Click += new System.EventHandler(this.bt_New_Click);
            // 
            // tb_MapWidth
            // 
            this.tb_MapWidth.Location = new System.Drawing.Point(804, 24);
            this.tb_MapWidth.Name = "tb_MapWidth";
            this.tb_MapWidth.Size = new System.Drawing.Size(50, 21);
            this.tb_MapWidth.TabIndex = 1;
            this.tb_MapWidth.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.tb_MapWidth_KeyPress);
            // 
            // tb_MapHeight
            // 
            this.tb_MapHeight.Location = new System.Drawing.Point(860, 24);
            this.tb_MapHeight.Name = "tb_MapHeight";
            this.tb_MapHeight.Size = new System.Drawing.Size(45, 21);
            this.tb_MapHeight.TabIndex = 2;
            this.tb_MapHeight.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.tb_MapHeight_KeyPress);
            // 
            // pn_MapViewer
            // 
            this.pn_MapViewer.BackColor = System.Drawing.Color.Black;
            this.pn_MapViewer.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pn_MapViewer.Controls.Add(this.pn_ToolSelect);
            this.pn_MapViewer.Location = new System.Drawing.Point(12, 9);
            this.pn_MapViewer.Name = "pn_MapViewer";
            this.pn_MapViewer.Size = new System.Drawing.Size(787, 787);
            this.pn_MapViewer.TabIndex = 3;
            this.pn_MapViewer.Click += new System.EventHandler(this.pn_MapViewer_Click);
            this.pn_MapViewer.Paint += new System.Windows.Forms.PaintEventHandler(this.pn_MapViewer_Paint);
            // 
            // pn_ToolSelect
            // 
            this.pn_ToolSelect.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.pn_ToolSelect.ForeColor = System.Drawing.Color.CornflowerBlue;
            this.pn_ToolSelect.Location = new System.Drawing.Point(792, 99);
            this.pn_ToolSelect.Name = "pn_ToolSelect";
            this.pn_ToolSelect.Size = new System.Drawing.Size(100, 100);
            this.pn_ToolSelect.TabIndex = 7;
            // 
            // lb_MapSize
            // 
            this.lb_MapSize.AutoSize = true;
            this.lb_MapSize.Location = new System.Drawing.Point(802, 9);
            this.lb_MapSize.Name = "lb_MapSize";
            this.lb_MapSize.Size = new System.Drawing.Size(103, 12);
            this.lb_MapSize.TabIndex = 4;
            this.lb_MapSize.Text = "Map Size = (0, 0)";
            // 
            // bt_SaveAs
            // 
            this.bt_SaveAs.Location = new System.Drawing.Point(856, 80);
            this.bt_SaveAs.Name = "bt_SaveAs";
            this.bt_SaveAs.Size = new System.Drawing.Size(49, 23);
            this.bt_SaveAs.TabIndex = 5;
            this.bt_SaveAs.Text = "Save As";
            this.bt_SaveAs.UseVisualStyleBackColor = true;
            this.bt_SaveAs.Click += new System.EventHandler(this.bt_SaveAs_Click);
            // 
            // bt_Open
            // 
            this.bt_Open.Location = new System.Drawing.Point(856, 51);
            this.bt_Open.Name = "bt_Open";
            this.bt_Open.Size = new System.Drawing.Size(49, 23);
            this.bt_Open.TabIndex = 6;
            this.bt_Open.Text = "Open";
            this.bt_Open.UseVisualStyleBackColor = true;
            this.bt_Open.Click += new System.EventHandler(this.bt_Open_Click);
            // 
            // rb_Pen
            // 
            this.rb_Pen.AutoSize = true;
            this.rb_Pen.Checked = true;
            this.rb_Pen.Location = new System.Drawing.Point(812, 109);
            this.rb_Pen.Name = "rb_Pen";
            this.rb_Pen.Size = new System.Drawing.Size(45, 16);
            this.rb_Pen.TabIndex = 7;
            this.rb_Pen.TabStop = true;
            this.rb_Pen.Text = "Pen";
            this.rb_Pen.UseVisualStyleBackColor = true;
            // 
            // rb_Line
            // 
            this.rb_Line.AutoSize = true;
            this.rb_Line.Location = new System.Drawing.Point(812, 132);
            this.rb_Line.Name = "rb_Line";
            this.rb_Line.Size = new System.Drawing.Size(47, 16);
            this.rb_Line.TabIndex = 8;
            this.rb_Line.Text = "Line";
            this.rb_Line.UseVisualStyleBackColor = true;
            // 
            // rb_Rectangle
            // 
            this.rb_Rectangle.AutoSize = true;
            this.rb_Rectangle.Location = new System.Drawing.Point(812, 155);
            this.rb_Rectangle.Name = "rb_Rectangle";
            this.rb_Rectangle.Size = new System.Drawing.Size(79, 16);
            this.rb_Rectangle.TabIndex = 9;
            this.rb_Rectangle.Text = "Rectangle";
            this.rb_Rectangle.UseVisualStyleBackColor = true;
            // 
            // rb_None
            // 
            this.rb_None.AutoSize = true;
            this.rb_None.Location = new System.Drawing.Point(812, 178);
            this.rb_None.Name = "rb_None";
            this.rb_None.Size = new System.Drawing.Size(53, 16);
            this.rb_None.TabIndex = 10;
            this.rb_None.Text = "None";
            this.rb_None.UseVisualStyleBackColor = true;
            // 
            // bt_Save
            // 
            this.bt_Save.Location = new System.Drawing.Point(804, 80);
            this.bt_Save.Name = "bt_Save";
            this.bt_Save.Size = new System.Drawing.Size(50, 23);
            this.bt_Save.TabIndex = 11;
            this.bt_Save.Text = "Save";
            this.bt_Save.UseVisualStyleBackColor = true;
            this.bt_Save.Click += new System.EventHandler(this.bt_Save_Click);
            // 
            // form_MapEditor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.ClientSize = new System.Drawing.Size(913, 808);
            this.Controls.Add(this.bt_Save);
            this.Controls.Add(this.rb_None);
            this.Controls.Add(this.rb_Rectangle);
            this.Controls.Add(this.rb_Line);
            this.Controls.Add(this.rb_Pen);
            this.Controls.Add(this.bt_Open);
            this.Controls.Add(this.bt_SaveAs);
            this.Controls.Add(this.lb_MapSize);
            this.Controls.Add(this.pn_MapViewer);
            this.Controls.Add(this.tb_MapHeight);
            this.Controls.Add(this.tb_MapWidth);
            this.Controls.Add(this.bt_New);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Fixed3D;
            this.Name = "form_MapEditor";
            this.Text = "Map Editor";
            this.pn_MapViewer.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button bt_New;
        private System.Windows.Forms.TextBox tb_MapWidth;
        private System.Windows.Forms.TextBox tb_MapHeight;
        private System.Windows.Forms.Panel pn_MapViewer;
        private System.Windows.Forms.Label lb_MapSize;
        private System.Windows.Forms.Button bt_SaveAs;
        private System.Windows.Forms.Button bt_Open;
        private System.Windows.Forms.Panel pn_ToolSelect;
        private System.Windows.Forms.RadioButton rb_Pen;
        private System.Windows.Forms.RadioButton rb_Line;
        private System.Windows.Forms.RadioButton rb_Rectangle;
        private System.Windows.Forms.RadioButton rb_None;
        private System.Windows.Forms.Button bt_Save;
    }
}

