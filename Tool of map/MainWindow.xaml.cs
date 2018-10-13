using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;

namespace Tool_of_map
{
    /// <summary>
    /// MainWindow.xaml에 대한 상호 작용 논리
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            tb_MapHeight.AcceptsReturn = false;
            tb_MapWidth.AcceptsReturn = false;
            tb_MapHeight.TextWrapping = TextWrapping.NoWrap;
            tb_MapWidth.TextWrapping = TextWrapping.NoWrap;

            Rectangle rect = new Rectangle();
            rect.Stroke = Brushes.LightBlue;
            rect.StrokeThickness = 2;

            Canvas.SetLeft(rect, 0);
            Canvas.SetTop(rect, 0);

            cv_MapView.Children.Add(rect);

        }

        private int MapWidth = 0;
        private int MapHeight = 0;

        private void tb_MapWidth_PreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            e.Handled = !int.TryParse(e.Text, out MapWidth);
            bt_MakeMap.IsEnabled = MapWidth > 0 && MapHeight > 0;
        }

        private void tb_MapHeight_PreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            e.Handled = !int.TryParse(e.Text, out MapHeight);
            bt_MakeMap.IsEnabled = MapWidth > 0 && MapHeight > 0;
        }

        private void bt_MakeMap_Click(object sender, RoutedEventArgs e)
        {
            lb_MapSize.Content = $"MapSize = ({MapWidth}, {MapHeight})";
        }
    }
}
